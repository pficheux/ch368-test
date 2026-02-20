KDIR= /lib/modules/$(shell uname -r)/build
PWD= $(shell pwd)

PROG= mem_test

obj-m := ch368_mem.o

all: $(PROG)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f led_blink *~ $(PROG)

