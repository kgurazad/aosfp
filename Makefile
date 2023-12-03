all:
	gcc -O3 kcp/main.c -o bin/kcp

debug:
	gcc -O0 -ggdb3 kcp/main.c -o bin/kcp

clean:
	-rm bin/kcp
