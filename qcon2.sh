#!/bin/bash
# TODO how to colour large result
# TODO allow user to change prompt colour
set -euo pipefail
type q vim perl vimcat.sh >/dev/null 2>&1 || { echo >&2 "cannot find one or more of q, vim, perl and vimcat.sh"; exit 1; }
hp=${1:-}
# TODO improve basic input check
if [[ -n `echo $hp | perl -wnE 'say /^[0-9]+$/g'` ]]; then
	hp="localhost:$hp"
elif [[ -z `echo $hp | perl -wnE 'say /^([\w.-]+:[0-9]+(?::[^:\s]*)?)(?::[^\s]*)?$/g'` ]]; then
	echo "usage: "
	echo -e "\t qcon2.sh port"
	echo -e "\t qcon2.sh host:port"
	echo -e "\t qcon2.sh host:port:user"
	echo -e "\t qcon2.sh host:port:user:pass\n"
	echo "start query with p) to enter"
	echo "multiple lines, send EOF to flush"
	echo "\\\\ to exit qcon2 itself"
	exit 1
fi
dhp=`echo $hp | perl -wnE 'say /^([\w.-]+:[0-9]+(?::[^:\s]*)?)(?::[^\s]*)?$/g'`

while true; do
	echo -ne '\033[1;32m'$dhp'>\033[0m'
	IFS=$' \t\n'	
	read -r text
	# paste/multi lines start with p)
	if [[ -n `echo $text | perl -wnE 'say /^p\).*/g'` ]]; then
		text=${text:2}' '$(cat)
	fi

	# \\ quits qcon2 itself, don't kill remote
	if [[ -n `echo $text | perl -wnE 'say /^(?:p\))?\s*\\\\\\\\.*/g'` ]]; then
		echo exiting, did not kill remote
		exit 0
	fi

	text=$(echo $text | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g')
	IFS=$'\0'
	res=$(echo "hsym[\`$\"$hp\"]\"$text\"" | q -c $(tput lines) $(tput cols))
	reswc=`echo "$res" | wc -l`
	# skip vimcat if result too big
	if [[ $reswc -lt 200 && -z `echo $res | perl -wnE 'say /\.\.$/gm'` ]]; then
		if [[ ${res:0:1} == '{' ]]; then
			vimcat.sh <(echo "$res")
		elif [[ ${res:0:2} == 'k)' ]]; then
			printf "k)"
			vimcat.sh <(echo "${res:2}")
		else
			echo "$res"
		fi
	else
		echo "$res"
	fi
done
exit 0
