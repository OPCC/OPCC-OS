cd $(dirname "$0")
nasm -f elf32 kernel.asm -o ./build/kasm.o
gcc -m32 -c kernel.c -o ./build/kc.o
ld -m elf_i386 -T link.ld -o ./build/kernel ./build/kasm.o ./build/kc.o

#Emulate Boot. Comment line below to disable emulator
apt-get install qemu-system-i386
qemu-system-i386 -kernel ./build/kernel
