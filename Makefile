FILES = ./build/kernel.asm.o ./build/kernel.o ./build/idt.o ./build/idt_c.o ./build/pic.o ./build/keyboard.o ./build/ata.o ./build/sfs.o ./build/libc.o ./build/edit.o ./build/vbe.o \
	./build/heap.o ./build/font_8x16.o ./build/terminal.o ./build/mouse.o ./build/desktop.o ./build/api_vbe.o ./build/kern_term.o ./build/api_keyboard.o ./build/api_mouse.o ./build/api_fs.o \
	./build/wm.o ./build/ipsfs.o ./build/ipscc.o ./build/stdio.o ./build/tcc.o ./build/tcc_runtime.o ./build/tcc_stdatomic.o ./build/tcc_atomic.o ./build/tcc_builtin.o ./build/tcc_pic86.o ./build/tcc_port.o ./build/tcc_alloca.o ./build/setjmp.o ./build/tcc_sys.o ./build/tcc_headers.o

FLAGS = -I./src/include -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -w -O0 \
        -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mstackrealign

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
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/libc.c -nostdinc -o ./build/libc.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/stdio.c -nostdinc -o ./build/stdio.o
	rm -rf ./build/tcc_headers
	mkdir -p ./build/tcc_headers/tcc
	cp -R ./src/include/. ./build/tcc_headers/
	cp -R ./src/tcc/tinycc/include/. ./build/tcc_headers/tcc/
	tar -cf ./build/tcc_headers.tar -C ./build/tcc_headers .
	i686-elf-objcopy --input-target=binary --output-target=elf32-i386 --binary-architecture=i386 build/tcc_headers.tar build/tcc_headers.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/libs/tcc_sys.c -nostdinc -o ./build/tcc_sys.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -DONE_SOURCE=1 -DIPSOS -DTCC_TARGET_I386 -DCONFIG_TCC_STATIC -DCONFIG_TCC_SEMLOCK=0 -DCONFIG_TCC_BACKTRACE=0 -DCONFIG_TCC_BCHECK=0 -DCONFIG_TCC_PREDEFS=1 -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include -c ./src/tcc/tinycc/libtcc.c -o ./build/tcc.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include -c ./src/tcc/tcc_port.c -o ./build/tcc_port.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include -c ./src/tcc/tinycc/lib/libtcc1.c -o ./build/tcc_runtime.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -c ./src/tcc/tinycc/lib/alloca.S -o ./build/tcc_alloca.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include -c ./src/tcc/tinycc/lib/builtin.c -o ./build/tcc_builtin.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include -c ./src/tcc/tinycc/lib/stdatomic.c -o ./build/tcc_stdatomic.o
	i686-elf-gcc -I./src $(FLAGS) -m32 -c ./src/tcc/tinycc/lib/atomic.S -o ./build/tcc_atomic.o
	i686-elf-gcc -I./src $(FLAGS) -m32 -c ./src/tcc/tinycc/lib/pic86.S -o ./build/tcc_pic86.o
	i686-elf-gcc -I./src $(FLAGS) -m32 -c ./src/kernel/setjmp.S -o ./build/setjmp.o
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
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/ipscc/ipscc.c -o ./build/ipscc.o
	i686-elf-ld -g -relocatable $(FILES) -o ./build/completeKernel.o
	i686-elf-gcc -m32 -nostdlib -T ./linkerScript.ld -o ./bin/kernel.elf ./build/completeKernel.o -lgcc
	i686-elf-objcopy -O binary ./bin/kernel.elf ./bin/kernel.bin
	@KERNEL_SIZE=$$(stat -c%s ./bin/kernel.bin); if [ $$KERNEL_SIZE -gt 917504 ]; then echo "kernel.bin exceeds the boot loader limit (896 KiB)"; exit 1; fi

	rm -f ./bin/os.bin
	dd if=./bin/boot.bin >> ./bin/os.bin
	dd if=./bin/kernel.bin >> ./bin/os.bin
	dd if=/dev/zero bs=512 count=8 >> ./bin/os.bin

	qemu-img create -f raw ipsos.img 10M
	dd if=bin/boot.bin of=ipsos.img bs=512 seek=0 conv=notrunc
	dd if=bin/kernel.bin of=ipsos.img bs=512 seek=1 conv=notrunc

clean:
	rm -f ./bin/*.bin ./bin/*.elf ./bin/*.o
	rm -f ./build/*.o ./build/tcc_headers.tar
	rm -rf ./build/tcc_headers
	rm -f ipsos.img

run:
	qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0 

rungdb:
	qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0 -s -S
