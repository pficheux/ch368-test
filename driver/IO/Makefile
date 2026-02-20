KDIR= /lib/modules/$(shell uname -r)/build
PWD= $(shell pwd)

obj-m := ch368_io.o

all: led_blink
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f led_blink *~

