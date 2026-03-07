KDIR= /lib/modules/$(shell uname -r)/build
PWD= $(shell pwd)

obj-m := ch368_io.o

PROG= led_blink d0_d3

all: $(PROG)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(PROG) *~

