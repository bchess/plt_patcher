all:
	gcc -g -fPIC -o theLib.so --shared theLib.c
	gcc -g -o main -ldl -rdynamic main.c patcher.c
