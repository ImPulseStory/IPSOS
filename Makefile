FILES = ./build/kernel.asm.o ./build/kernel.o ./build/idt.o ./build/idt_c.o ./build/pic.o ./build/keyboard.o ./build/ata.o
FLAGS = -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 \
        -fno-stack-protector -mno-red-zone -mstackrealign

all:
	nasm -f bin ./src/boot.asm -o ./bin/boot.bin
	nasm -f elf32 -g ./src/kernel.asm -o ./build/kernel.asm.o
	nasm -f elf32 -g ./src/idt.asm -o ./build/idt.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel.c -o ./build/kernel.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/idt.c -o ./build/idt_c.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/pic.c -o ./build/pic.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/keyboard.c -o ./build/keyboard.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/ata.c -o ./build/ata.o
	i686-elf-ld -g -relocatable $(FILES) -o ./build/completeKernel.o
	i686-elf-ld -T ./linkerScript.ld -o ./bin/kernel.elf ./build/completeKernel.o
	i686-elf-objcopy -O binary ./bin/kernel.elf ./bin/kernel.bin

	rm -f ./bin/os.bin
	dd if=./bin/boot.bin >> ./bin/os.bin
	dd if=./bin/kernel.bin >> ./bin/os.bin
	dd if=/dev/zero bs=512 count=8 >> ./bin/os.bin

clean:
	rm -f ./bin/boot.bin
	rm -f ./bin/kernel.bin
	rm -f ./bin/kernel.elf
	rm -f ./bin/os.bin
	rm -f ./build/kernel.asm.o
	rm -f ./build/kernel.o
	rm -f ./build/idt.o
	rm -f ./build/idt_c.o
	rm -f ./build/completeKernel.o

run:
	qemu-system-x86_64 \
  -drive file=./bin/os.bin,format=raw,if=ide,index=0 \
  -drive file=disk.img,format=raw,if=ide,index=1

rungdb:
	qemu-system-x86_64 \
  -drive file=./bin/os.bin,format=raw,if=ide,index=0 \
  -drive file=disk.img,format=raw,if=ide,index=1 -s -S