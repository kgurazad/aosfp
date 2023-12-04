LIBS=-luring

all:
	gcc -O3 kcp/main.c $(LIBS) -o bin/kcp

debug:
	gcc -O0 -ggdb3 kcp/main.c $(LIBS) -o bin/kcp

clean:
	-rm bin/kcp
