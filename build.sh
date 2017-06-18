#!/bin/bash
cd $(dirname "$0")
nasm -f elf32 kernel.asm -o ./build/boot/kasm.o
gcc -m32 -c kernel.c -o ./build/boot/kc.o
ld -m elf_i386 -T link.ld -o ./build/boot/kernel ./build/boot/kasm.o ./build/boot/kc.o

if [ "$1" == "run" ];
then
apt-get install qemu-system-i386
qemu-system-i386 -kernel ./build/boot/kernel
fi
