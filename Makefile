CFLAGS =-Wall -g3 -Wno-return-type -lpthread -lncurses
LDFLAGS =-L/usr/local/opt/ncurses/lib
#CFLAGS =-Wall -Wno-return-type -g3 -lpthread -lncurses

all: main runner

main:
	gcc $(CFLAGS) $(LDFLAGS) -o main main.c

runner:
	gcc $(CFLAGS) $(LDFLAGS) -o runner runner.c


clean:
	rm -rf ./main ./core* ./runner
