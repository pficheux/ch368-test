CC=gcc

CFLAGS= -g -I ../../ch36x_linux/lib/ -Wall

PROGS= l1_l4 flip_flop d0_d3

all: $(PROGS)

.c: 
	$(CC) $(CFLAGS) -o $@ $@.c -L ../../ch36x_linux/lib -lch36x

clean:
	rm -f $(PROGS)  *~ 
