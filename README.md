similiar functionality to kx's qcon tool with the following extras:

- display lambdas using vim syntax highlighting (i like colours)
- paste multiple lines as one query (useful for lambdas)
- allows quick switching between instances, driven by a config

uses the following:

- slightly modified vimcat, for the highlighting (https://github.com/vim-scripts/vimcat)
- ncurses and cdk (http://invisible-island.net/cdk/)
- readline library

to compile, install cdk, ncurses, readline and readline-devel package and then:

gcc qcon2.c -o qcon2 -L/path/to/cdk/cdk-5.0-20141106 -lcdk -lncurses -lreadline

or use pkg-config to help

usage:

patrick@ubuntu:~/git/qcon2$ ./qcon2

usage:qcon2 host:port

^\          - p)aste mode (SIGQUIT)

p)aste mode - send EOF after blank line to flush

env QCONVIM - path to vimcat script (optional, requires vim + q highlight)

env QCONCFG - path to config file   (optional, requires libncurses)

config file - one per line: NAME HOST:PORT. max 9000 entries of 256 char each

;<ENTER>    - change connections using config


example:

QCONCFG=./exampleconfig QCONVIM=./vimcat.sh ./qcon2 localhost:1337
