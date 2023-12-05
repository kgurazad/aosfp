LIBS=-luring

all:
	gcc -O3 kcp/*.c $(LIBS) -o bin/kcp

debug:
	gcc -DVERBOSE -O0 -ggdb3 kcp/*.c $(LIBS) -o bin/kcp

clean:
	-rm bin/kcp
