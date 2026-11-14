FILES = ./build/kernel.asm.o ./build/kernel.o ./build/idt.o ./build/idt_c.o ./build/pic.o ./build/keyboard.o ./build/ata.o ./build/sfs.o ./build/libc.o ./build/edit.o ./build/vbe.o \
	./build/heap.o ./build/font_8x16.o ./build/terminal.o ./build/mouse.o ./build/desktop.o ./build/api_vbe.o ./build/kern_term.o ./build/api_keyboard.o ./build/api_mouse.o ./build/api_fs.o \
	./build/wm.o ./build/ipsfs.o

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
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/fs/sfs.c -o ./build/sfs.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/libc.c -o ./build/libc.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/apps/edit/edit.c -o ./build/edit.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/drivers/vbe.c -o ./build/vbe.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/heap.c -o ./build/heap.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/font_8x16.c -o ./build/font_8x16.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/apps/terminal/terminal.c -o ./build/terminal.o	
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/drivers/mouse.c -o ./build/mouse.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/apps/IPDesktop/desktop.c -o ./build/desktop.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/api/api_vbe.c -o ./build/api_vbe.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/kern_term.c -o ./build/kern_term.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/api/api_keyboard.c -o ./build/api_keyboard.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/api/api_mouse.c -o ./build/api_mouse.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/api/api_fs.c -o ./build/api_fs.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/apps/IPDesktop/wm.c -o ./build/wm.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/fs/ipsfs.c -o ./build/ipsfs.o
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
	rm -f ./bin/*.bin ./bin/*.elf ./bin/*.o
	rm -f ./build/*.o
	rm -f ipsos.img

run:
	qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0 

rungdb:
	qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0 -s -S