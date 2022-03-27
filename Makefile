CFLAGS =-Wall -g3 -Wno-return-type -lpthread -lncurses
LDFLAGS =-L/usr/local/opt/ncurses/lib
#CFLAGS =-Wall -Wno-return-type -g3 -lpthread -lncurses

all: main

main:
	gcc $(CFLAGS) $(LDFLAGS) -o main main.c

clean:
	rm -rf ./main ./core*
