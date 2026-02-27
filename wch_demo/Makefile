CC=gcc

CFLAGS= -g -I ../../ch36x_linux/lib/

PROGS= l1_l4 sdx

all: $(PROGS)

.c: 
	$(CC) $(CFLAGS) -o $@ $@.c -L ../../ch36x_linux/lib -lch36x

clean:
	rm -f $(PROGS)  *~ 
