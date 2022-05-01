obj-m := hello.o

# Linux kernel version.
KVER = linux-5.18-rc3

all: 
	make -C $(HOME)/uml/$(KVER) ARCH=um M=$(PWD) modules

clean:
	@rm -f *~ *.o *.ko *.mod.* *.mod modules.* Module.* .*.cmd
