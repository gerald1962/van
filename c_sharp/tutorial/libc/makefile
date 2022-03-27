
.PHONY: all lib exe

all: lib exe

lib: libc.c
	gcc libc.c -g -shared -o libc.so

exe: libc.cs
	mcs libc.cs
