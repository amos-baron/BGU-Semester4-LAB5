CC = gcc
LD = ld
NASM = nasm

CFLAGS = -m32 -Wall -fno-pie -fno-stack-protector -c
LDFLAGS = -m elf_i386 -nostdlib -T linking_script
ASMFLAGS = -f elf32

all: loader

loader: loader.o start.o startup.o
	$(LD) $(LDFLAGS) -o loader start.o startup.o loader.o

loader.o: loader.c
	$(CC) $(CFLAGS) loader.c -o loader.o

start.o: start.s
	$(NASM) $(ASMFLAGS) start.s -o start.o

startup.o: startup.s
	$(NASM) $(ASMFLAGS) startup.s -o startup.o

.PHONY: clean
clean:
	rm -f loader loader.o start.o startup.o
