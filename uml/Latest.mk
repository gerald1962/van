obj-m := hello.o

all: 
	make -C $(HOME)/github/linux ARCH=um M=$(PWD) modules

clean:
	@rm -f *~ *.o *.ko *.mod.* *.mod modules.* Module.* .*.cmd
