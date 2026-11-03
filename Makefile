FILES = ./build/kernel.asm.o ./build/kernel.o ./build/idt.o ./build/idt_c.o ./build/pic.o ./build/keyboard.o ./build/ata.o ./build/shell.o ./build/sfs.o ./build/libc.o ./build/edit.o
FLAGS = -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 \
        -fno-stack-protector -mno-red-zone -mstackrealign

all:
	nasm -f bin ./src/boot/boot.asm -o ./bin/boot.bin
	nasm -f elf32 -g ./src/kernel/kernel.asm -o ./build/kernel.asm.o
	nasm -f elf32 -g ./src/cpu/idt.asm -o ./build/idt.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/kernel.c -o ./build/kernel.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/cpu/idt.c -o ./build/idt_c.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/cpu/pic.c -o ./build/pic.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/drivers/keyboard.c -o ./build/keyboard.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/drivers/ata.c -o ./build/ata.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/shell.c -o ./build/shell.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/fs/sfs.c -o ./build/sfs.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/libc.c -o ./build/libc.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/apps/edit/edit.c -o ./build/edit.o
	i686-elf-ld -g -relocatable $(FILES) -o ./build/completeKernel.o
	i686-elf-ld -T ./linkerScript.ld -o ./bin/kernel.elf ./build/completeKernel.o
	i686-elf-objcopy -O binary ./bin/kernel.elf ./bin/kernel.bin

	rm -f ./bin/os.bin
	dd if=./bin/boot.bin >> ./bin/os.bin
	dd if=./bin/kernel.bin >> ./bin/os.bin
	dd if=/dev/zero bs=512 count=8 >> ./bin/os.bin

	qemu-img create -f raw ipsos.img 10M
	dd if=bin/boot.bin of=ipsos.img bs=512 seek=0 conv=notrunc
	dd if=bin/kernel.bin of=ipsos.img bs=512 seek=1 conv=notrunc

clean:
	rm -f ./bin/boot.bin
	rm -f ./bin/kernel.bin
	rm -f ./bin/kernel.elf
	rm -f ./bin/os.bin
	rm -f ./build/kernel.asm.o
	rm -f ./build/kernel.o
	rm -f ./build/idt.o
	rm -f ./build/keyboard.o
	rm -f ./build/pic.o
	rm -f ./build/ata.o
	rm -f ./build/idt_c.o
	rm -f ./build/completeKernel.o
	rm -f ./build/sfs.o
	rm -f ./build/shell.o
	rm -f ipsos.img

run:
	qemu-system-x86_64 \
  -drive file=./bin/os.bin,format=raw,if=ide,index=0 \
  -drive file=disk.img,format=raw,if=ide,index=1

run_test:
	qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0

rungdb:
	qemu-system-x86_64 \
  -drive file=./bin/os.bin,format=raw,if=ide,index=0 \
  -drive file=disk.img,format=raw,if=ide,index=1 -s -S