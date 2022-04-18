obj-m := hello.o

EXTRA_CFLAGS=-I$(PWD)/inc


all:
	make -C /home/gerald/uml/linux-5.18-rc3 ARCH=i386 M=`pwd` modules
