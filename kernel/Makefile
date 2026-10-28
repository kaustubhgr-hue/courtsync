# Kernel module Makefile (Linux only). The real build is done by the kernel's
# own build system (kbuild) in KDIR; we just tell it which object to build.
obj-m += scoreboard.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: all clean
