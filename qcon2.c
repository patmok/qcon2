#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <memory.h>
#include <regex.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cdk.h>
#include <readline/readline.h>
#include <readline/history.h>
#define  MAXHOST  100
#define  MAXHIST  100
#define  MAXCFG   9000
#define  MAXINPUT 1500
#define  MAXRES   8192
#define  MAXQUERY 8192
#define  IPLEN    15 // xxx.xxx.xxx.xxx
// TODO curses error?
// TODO user/pass - useful?
// TODO history
char QRESULT[]="/tmp/qresultXXXXXX"; // for vimcat
int  PASTE=0;
jmp_buf JUMPBUFFER;
CDKSCREEN *CDKSCREEN_P=NULL;
int MENU=0; // indicates whether we are in curse menu
CDKALPHALIST *ALPHALIST=NULL;

void usage()
{
	printf("usage:qcon2 host:port\n");
	printf("^\\          - p)aste mode (SIGQUIT)\n");
	printf("p)aste mode - send EOF after blank line to flush\n");
	printf("env QCONVIM - path to vimcat script (optional, requires vim + q highlight)\n");
	printf("env QCONCFG - path to config file   (optional, requires libncurses)\n");
	printf("config file - one per line: NAME HOST:PORT. max 9000 entries of 256 char each\n");
	printf(";<ENTER>    - change connections using config\n");
	exit(1);
}

void cleanup(int x)
{
	unlink(QRESULT);
	if(CDKSCREEN_P!=NULL){
		destroyCDKAlphalist(ALPHALIST);
		destroyCDKScreen(CDKSCREEN_P);
		endCDK();
	}
	clear_history();
	exit(0);
}

void toggle(int x)
{
	if(!MENU){
		PASTE=!PASTE;
		printf("\n");
		siglongjmp(JUMPBUFFER,1);
	}
}

void getHp(char *hp,char *host,int *port) // ":"vs
{
	char *token=strtok(hp,":");
	memcpy(host,token,strlen(token)+1);
	token=strtok(NULL,":");
	*port=atoi(token);
}

int readCfg(char *file,char *hostports[])
{
	FILE *fp=fopen(file,"r");
	if(fp==NULL){
		perror("fopen");
		return 0;
	}
	size_t n;
	char *line=NULL;
	int nhost=0;
	while(nhost<MAXCFG && getline(&line,&n,fp)!=-1){
		if(*line!='\n')    // ignore empty newline
			hostports[nhost++]=line;	
		else
			free(line);
		line=NULL;
	};
	free(line);
	fclose(fp);
	return nhost;
}

int createConnection(struct sockaddr_in *server,char *host,int port)
{
	memset(server,0,sizeof(struct sockaddr_in));
        char *ip=(char*)malloc(IPLEN);
	if(ip==NULL){
		perror("failed malloc for ip");
		exit(1);
	}
        struct hostent *hent=gethostbyname(host);
	if(hent==NULL){
		printf("'host\n");
		return 1;
	}
        inet_ntop(AF_INET,(void*)hent->h_addr_list[0],ip,IPLEN);
        server->sin_family=AF_INET;
        server->sin_port=htons(port);
        server->sin_addr.s_addr=inet_addr(ip);
        free(ip);
	return 0;
}

int queryQ(int soc,char *query,int ninput,char *res)
{
	sendto(soc,query,5+ninput,0,NULL,0);
	shutdown(soc,SHUT_WR);
	memset(res,0,MAXRES);
	int n=recv(soc,res,8192-2,0);
	if(n==-1)
		perror("recv");
	return n;
}

int writeQRESULT(char *res,int resn)
{
	regex_t regex;
	regcomp(&regex,".*\\.\\.$.*",REG_EXTENDED);
	// if truncated and has strings, it looks bad - skip it
	if(!regexec(&regex,res,0,NULL,0) && strchr(res,'\"')!=NULL){
		regfree(&regex);
		return 1;
	}
	regfree(&regex);
	int fd=open(QRESULT,O_RDONLY | O_WRONLY | O_TRUNC);
	if(fd==-1){
		perror("open");
		return 1;
	}
	int w=write(fd,*res=='k' && res[1]==')'? res+2 : res,*res=='k' && res[1]==')'? resn-2: resn);
	if(w==-1){
		perror("write");
                return 1;
	}
	if(*res=='k'){ // work around vimcat
		printf("k)");
		fflush(stdout);
	}
	close(fd);
	return 0;
}

void displayRes(char *res,int resn,char *vimcat,char *cmd)
{
	if(vimcat!=NULL && *res=='{' && !writeQRESULT(res,resn) && -1!=system(cmd))
		; // use vimcat if its a lambda
	else if(vimcat!=NULL && *res=='k' && res[1]==')' && !writeQRESULT(res,resn) && -1!=system(cmd))
		; // k)
	else{
		if(res[resn==0? 0 : resn-1]!='\n')
			res[resn]='\n';
		printf("%s",res);
	}
}

void initMenu(char *hostports[],int nhostport) // TODO can fail
{
	WINDOW *cursesWin=initscr();
	CDKSCREEN_P=initCDKScreen(cursesWin);
	initCDKColor();
	ALPHALIST=newCDKAlphalist(CDKSCREEN_P,CENTER,CENTER,
				  0,0," </B><ESC><!B>\t  cancel\n </B>^U<!B>\t  clear\n </B><TAB><!B>\t  completion\n","</B> process: ",
				  (CDK_CSTRING*)hostports,nhostport,
				  '_',A_REVERSE,TRUE,FALSE);
}

void showMenu(struct sockaddr_in *server,char *host,int *port,char *hostports[],int nhostport)
{
	MENU=1;
	int bad=0;
	if(CDKSCREEN_P==NULL)
		initMenu(hostports,nhostport);
	else
		refresh();
	setCDKAlphalistCurrentItem(ALPHALIST,0);
	char *res=activateCDKAlphalist(ALPHALIST,NULL);
	if(ALPHALIST->exitType!=vESCAPE_HIT){
		regex_t regex; // basic regex sanity check
		regcomp(&regex,".+ .+:[0-9]+",REG_EXTENDED);
		if(!regexec(&regex,res,0,NULL,0)){
	 		char *token=strtok(res," ");
	        	token=strtok(NULL," ");
			getHp(token,host,port);
		}else
			bad=1;
		regfree(&regex);
	}
	def_prog_mode();
	endwin();
	if(bad)
		printf("'bad input, NAME HOST:PORT\n");
	MENU=0;
}

char *initVimcat(char **vimcat)
{
	if(*vimcat!=NULL){
		int fd=mkstemp(QRESULT);
		if(fd==-1){ // don't use vimcat without temp file
			perror("mkstemp");
			*vimcat=NULL;
			return NULL;
		}
		close(fd);
		char *cmd=(char*)calloc(256,1); // e.g. ./vimcat.sh /tmp/qresult123456
		strncat(cmd,*vimcat,strlen(*vimcat));
                strncat(cmd," ",1);
                strncat(cmd,QRESULT,strlen(QRESULT));
		return cmd;
	}else
		return NULL;
}

void checkArg(char *argv) // to prevent strtok seg faulting
{
	regex_t regex;
	regcomp(&regex,".+:[0-9]+",REG_EXTENDED);
	if(regexec(&regex,argv,0,NULL,0))
		usage();
	regfree(&regex);
}

int main(int argc,char *argv[])
{
	char host[256];
	int port=0;
	if(argc!=2)
		usage();
	else{
		checkArg(argv[1]);
		getHp(argv[1],host,&port);
	}
	char *vimcat=getenv("QCONVIM");
	char *cmd=initVimcat(&vimcat);
	char *cfg=getenv("QCONCFG");
	char *hostports[MAXCFG]; // for storing config entries 
	int nhostport=0;

	if(cfg!=NULL)
		nhostport=readCfg(cfg,hostports);
	if(nhostport==0)
		cfg=NULL;
	// connection details
	int soc;
	int optval=1;
	struct sockaddr_in server;

	signal(SIGINT,cleanup);       // ^C cleanup and quit
	signal(SIGQUIT,toggle);       // ^\ to toggle paste

	char query[MAXQUERY];         // eventually send this to q
	char res[MAXRES];             // NOTE free - q result
	char input[MAXINPUT];         // user stdin input
	char *rline=NULL;             // readline output
	
	strcpy(query,"qcon\0");
	using_history();
	stifle_history(MAXHIST);
	while(1){
		sigsetjmp(JUMPBUFFER,1);
		char prompt[256];     // size check?
		sprintf(prompt,"%s%s:%d>",PASTE? "p)" : "",host,port);
		memset(input,0,MAXINPUT);
		int rlength=0;        // readline length
		rline=readline(prompt);
		if(!PASTE){
			if(rline==NULL) // ignore EOF
				continue;
			rlength=strlen(rline);
			if(rlength>MAXINPUT-2){ // \n and \0
				printf("'input too long\n");
				free(rline);
				continue;
			}
			strncat(input,rline,rlength);
			input[strlen(input)]='\n';
			if(*rline)
				add_history(rline);
			free(rline);
		}
		if(PASTE){
			if(rline){
				rlength=strlen(rline);
				if(rlength>MAXINPUT-2){
					printf("'input too long\n");
					free(rline);
					continue;
				}
				if(rlength==0)
					input[0]='\n';
				else{
					strncat(input,rline,rlength);
					input[strlen(input)]='\n';
				}
				rlength+=1;
				if(*rline)
					add_history(rline);
				free(rline);
			}
			while(rline!=NULL){
				rline=readline("");
				if(rline){
					rlength+=strlen(rline)+1;
					if(rlength>MAXINPUT-1){
						printf("'input too long\n");
						free(rline);
						break;
					};
					strncat(input,rline,strlen(rline));
					input[strlen(input)]='\n';
					if(*rline)
						add_history(rline);
					free(rline);
				}
			}
			printf("\n");
		}
		if(rlength>MAXINPUT-1)
			continue;
		// \\ to exit, don't send to q
		if(strcmp("\\\\",input)==0 || strcmp("\\\\\n",input)==0)
			break;
		if(input!=NULL && *input==';'){
			if(cfg!=NULL)
				showMenu(&server,host,&port,hostports,nhostport);
			else
				printf("'QCONCFG\n");
			continue;
		}
		if(createConnection(&server,host,port))
			continue;	
		// calculate length of input, handle \n
		int ninput=strlen(input);
		ninput+=input[strlen(input)-1]=='\n' ? 0 : 1;
		strcpy(query+5,input);
		query[4+ninput]='\0';
		soc=socket(PF_INET,SOCK_STREAM,IPPROTO_IP);
		setsockopt(soc,SOL_SOCKET,SO_KEEPALIVE,&optval,4);	
		// connect and query
		if(connect(soc,(struct sockaddr*)&server,16)==-1)
			perror("conn");
		else{
			int resn=queryQ(soc,query,ninput,res);
			if(resn==-1){
				close(soc);
				continue;
			}
			displayRes(res,resn,vimcat,cmd);
			close(soc);
		}
		PASTE=0; // just my preference
	}
	int i;
	for(i=0;i<nhostport;i++)
		free(hostports[i]);
	free(cmd);
	unlink(QRESULT);
	return 0;
}
