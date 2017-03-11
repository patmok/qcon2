qcon2 is a terminal based front end to kdb+ (based on kx's qcon). it enables you to type in queries interactively, issue them to kdb+, and see the query results. it's a simple wrapper around q using single shot ipc:

- display lambdas using vim syntax highlighting
- paste multiple lines as one query

![alt tag](https://raw.githubusercontent.com/patmok/qcon2/master/Capture.PNG)

uses the following:

- very slightly modified [vimcat](https://github.com/vim-scripts/vimcat) for the highlighting 

#### install
- download [qcon2.sh](https://raw.githubusercontent.com/patmok/qcon2/master/qcon2.sh) and [vimcat.sh](https://raw.githubusercontent.com/patmok/qcon2/master/vimcat.sh), chmod u+x and put it somewhere under $PATH
- optional, alias qcon2='rlwrap bash qcon2.sh'

#### example
```
patrick@ubuntu:~$ type qcon2
qcon2 is aliased to `rlwrap bash qcon2.sh'
patrick@ubuntu:~$ qcon2
usage:
         qcon2.sh port
         qcon2.sh host:port
         qcon2.sh host:port:user
         qcon2.sh host:port:user:pass

start query with p) to enter
multiple lines, send EOF to flush
\\ to exit qcon2 itself
patrick@ubuntu:~$ qcon2 1234
localhost:1234> 5#.Q
    | ::
k   | 3.3
host| ![-12]
addr| ![-13]
gc  | ![-20]
localhost:1234> til 5
0 1 2 3 4
```

multi line
```
localhost:1234> p)f:{x+y
+1}

localhost:1234> f
{x+y +1}
localhost:1234> f[1;2]
4

```

#### dependancies

- q
- vim
- perl

#### change log

- 2017.03.11, reworked so it's just a shell script instead of the old C binary. this means you need q to use it but no longer dependant on readline, ncurse etc and it's much easier to install

