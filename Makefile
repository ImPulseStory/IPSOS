V ?= 0

ifeq ($(V),1)
Q =
say =
else
Q = @
say = @printf '  %-9s %s\n' '$(1)' '$(2)'
endif

CC = i686-elf-gcc
LD = i686-elf-ld
NASM = nasm
OBJCOPY = i686-elf-objcopy

FLAGS = -I./src/include -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -w -O0 \
        -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mstackrealign

FILES = \
	./build/kernel.asm.o \
	./build/kernel.o \
	./build/idt.o \
	./build/idt_c.o \
	./build/pic.o \
	./build/pit.o \
	./build/task.o \
	./build/exec.o \
	./build/keyboard.o \
	./build/ata.o \
	./build/sfs.o \
	./build/libc.o \
	./build/stdio.o \
	./build/edit.o \
	./build/vbe.o \
	./build/heap.o \
	./build/font_8x16.o \
	./build/terminal.o \
	./build/mouse.o \
	./build/desktop.o \
	./build/api_vbe.o \
	./build/kern_term.o \
	./build/api_keyboard.o \
	./build/api_mouse.o \
	./build/api_fs.o \
	./build/wm.o \
	./build/ipsfs.o \
	./build/ipscc.o \
	./build/tcc.o \
	./build/tcc_runtime.o \
	./build/tcc_stdatomic.o \
	./build/tcc_atomic.o \
	./build/tcc_builtin.o \
	./build/tcc_pic86.o \
	./build/tcc_port.o \
	./build/tcc_alloca.o \
	./build/setjmp.o \
	./build/tcc_sys.o \
	./build/tcc_headers.o

.PHONY: all clean run rungdb disk-update disk-init format-disk

all: disk-update

./build ./bin:
	$(call say,MKDIR,$@)
	$(Q)mkdir -p $@

define compile
$(call say,CC,$(1))
$(Q)$(CC) -I./src $(FLAGS) $(3) -std=gnu99 -c $(1) -o $(2)
endef

define assemble
$(call say,AS,$(1))
$(Q)$(NASM) $(2) $(1) -o $(3)
endef

./bin/boot.bin: ./src/boot/boot.asm | ./bin
	$(call assemble,$<,-f bin,$@)

./build/kernel.asm.o: ./src/kernel/kernel.asm | ./build
	$(call assemble,$<,-f elf32 -g,$@)

./build/idt.o: ./src/cpu/idt.asm | ./build
	$(call assemble,$<,-f elf32 -g,$@)

./build/kernel.o: ./src/kernel/kernel.c | ./build
	$(call compile,$<,$@)

./build/idt_c.o: ./src/cpu/idt.c | ./build
	$(call compile,$<,$@)

./build/pic.o: ./src/cpu/pic.c | ./build
	$(call compile,$<,$@)

./build/pit.o: ./src/drivers/pit.c | ./build
	$(call compile,$<,$@)

./build/task.o: ./src/kernel/task.c | ./build
	$(call compile,$<,$@)

./build/exec.o: ./src/kernel/exec.c | ./build
	$(call compile,$<,$@)

./build/keyboard.o: ./src/drivers/keyboard.c | ./build
	$(call compile,$<,$@)

./build/ata.o: ./src/drivers/ata.c | ./build
	$(call compile,$<,$@)

./build/sfs.o: ./src/fs/sfs.c | ./build
	$(call compile,$<,$@)

./build/libc.o: ./src/libs/libc.c | ./build
	$(call compile,$<,$@,-nostdinc)

./build/stdio.o: ./src/libs/stdio.c | ./build
	$(call compile,$<,$@,-nostdinc)

./build/edit.o: ./src/apps/edit/edit.c | ./build
	$(call compile,$<,$@)

./build/vbe.o: ./src/drivers/vbe.c | ./build
	$(call compile,$<,$@)

./build/heap.o: ./src/kernel/heap.c | ./build
	$(call compile,$<,$@)

./build/font_8x16.o: ./src/libs/font_8x16.c | ./build
	$(call compile,$<,$@)

./build/terminal.o: ./src/apps/terminal/terminal.c | ./build
	$(call compile,$<,$@)

./build/mouse.o: ./src/drivers/mouse.c | ./build
	$(call compile,$<,$@)

./build/desktop.o: ./src/apps/IPDesktop/desktop.c | ./build
	$(call compile,$<,$@)

./build/api_vbe.o: ./src/api/api_vbe.c | ./build
	$(call compile,$<,$@)

./build/kern_term.o: ./src/kernel/kern_term.c | ./build
	$(call compile,$<,$@)

./build/api_keyboard.o: ./src/api/api_keyboard.c | ./build
	$(call compile,$<,$@)

./build/api_mouse.o: ./src/api/api_mouse.c | ./build
	$(call compile,$<,$@)

./build/api_fs.o: ./src/api/api_fs.c | ./build
	$(call compile,$<,$@)

./build/wm.o: ./src/apps/IPDesktop/wm.c | ./build
	$(call compile,$<,$@)

./build/ipsfs.o: ./src/fs/ipsfs.c | ./build
	$(call compile,$<,$@)

./build/ipscc.o: ./src/ipscc/ipscc.c | ./build
	$(call compile,$<,$@)

./build/tcc_headers.tar: | ./build
	$(call say,HEADERS,TCC headers)
	$(Q)rm -rf ./build/tcc_headers
	$(Q)mkdir -p ./build/tcc_headers/tcc
	$(Q)cp -R ./src/include/. ./build/tcc_headers/
	$(Q)cp -R ./src/tcc/tinycc/include/. ./build/tcc_headers/tcc/
	$(Q)tar -cf $@ -C ./build/tcc_headers .

./build/tcc_headers.o: ./build/tcc_headers.tar
	$(call say,OBJCOPY,TCC headers)
	$(Q)$(OBJCOPY) --input-target=binary --output-target=elf32-i386 --binary-architecture=i386 $< $@

./build/tcc_sys.o: ./src/libs/tcc_sys.c | ./build
	$(call compile,$<,$@,-nostdinc)

./build/tcc.o: ./src/tcc/tinycc/libtcc.c | ./build
	$(call compile,$<,$@,-Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -DONE_SOURCE=1 -DIPSOS -DTCC_TARGET_I386 -DCONFIG_TCC_STATIC -DCONFIG_TCC_SEMLOCK=0 -DCONFIG_TCC_BACKTRACE=0 -DCONFIG_TCC_BCHECK=0 -DCONFIG_TCC_PREDEFS=1 -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include)

./build/tcc_port.o: ./src/tcc/tcc_port.c | ./build
	$(call compile,$<,$@,-Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include)

./build/tcc_runtime.o: ./src/tcc/tinycc/lib/libtcc1.c | ./build
	$(call compile,$<,$@,-Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include)

./build/tcc_alloca.o: ./src/tcc/tinycc/lib/alloca.S | ./build
	$(call say,AS,$<)
	$(Q)$(CC) -I./src $(FLAGS) -Os -fno-pic -fno-pie -c $< -o $@

./build/tcc_builtin.o: ./src/tcc/tinycc/lib/builtin.c | ./build
	$(call compile,$<,$@,-Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include)

./build/tcc_stdatomic.o: ./src/tcc/tinycc/lib/stdatomic.c | ./build
	$(call compile,$<,$@,-Os -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -I./src/tcc/tinycc -I./src/tcc/tinycc/include)

./build/tcc_atomic.o: ./src/tcc/tinycc/lib/atomic.S | ./build
	$(call say,AS,$<)
	$(Q)$(CC) -I./src $(FLAGS) -m32 -c $< -o $@

./build/tcc_pic86.o: ./src/tcc/tinycc/lib/pic86.S | ./build
	$(call say,AS,$<)
	$(Q)$(CC) -I./src $(FLAGS) -m32 -c $< -o $@

./build/setjmp.o: ./src/kernel/setjmp.S | ./build
	$(call say,AS,$<)
	$(Q)$(CC) -I./src $(FLAGS) -m32 -c $< -o $@

./build/completeKernel.o: $(FILES)
	$(call say,LD,completeKernel.o)
	$(Q)$(LD) -g -relocatable $(FILES) -o $@

./bin/kernel.elf: ./build/completeKernel.o ./linkerScript.ld | ./bin
	$(call say,LD,kernel.elf)
	$(Q)$(CC) -m32 -nostdlib -T ./linkerScript.ld -o $@ $< -lgcc

./bin/kernel.bin: ./bin/kernel.elf
	$(call say,OBJCOPY,kernel.bin)
	$(Q)$(OBJCOPY) -O binary $< $@
	$(Q)KERNEL_SIZE=$$(stat -c%s $@); if [ $$KERNEL_SIZE -gt 917504 ]; then echo "kernel.bin exceeds the boot loader limit (896 KiB)"; exit 1; fi

./bin/os.bin: ./bin/boot.bin ./bin/kernel.bin
	$(call say,CAT,os.bin)
	$(Q)rm -f $@
	$(Q)dd if=./bin/boot.bin of=$@ status=none
	$(Q)dd if=./bin/kernel.bin of=$@ oflag=append conv=notrunc status=none
	$(Q)dd if=/dev/zero bs=512 count=8 of=$@ oflag=append conv=notrunc status=none

disk-init:
	$(call say,IMG,ipsos.img)
	$(Q)test -f ipsos.img || qemu-img create -f raw ipsos.img 10M

disk-update: ./bin/boot.bin ./bin/kernel.bin ./bin/os.bin | disk-init
	$(call say,DD,boot.bin -> ipsos.img)
	$(Q)dd if=./bin/boot.bin of=ipsos.img bs=512 seek=0 conv=notrunc status=none
	$(call say,DD,kernel.bin -> ipsos.img)
	$(Q)dd if=./bin/kernel.bin of=ipsos.img bs=512 seek=1 conv=notrunc status=none

format-disk:
	$(call say,FORMAT,ipsos.img)
	$(Q)rm -f ipsos.img
	$(Q)qemu-img create -f raw ipsos.img 10M

clean:
	$(call say,CLEAN,build and bin)
	$(Q)rm -f ./bin/*.bin ./bin/*.elf ./bin/*.o
	$(Q)rm -f ./build/*.o ./build/tcc_headers.tar
	$(Q)rm -rf ./build/tcc_headers

run: all
	$(call say,QEMU,IPSOS)
	$(Q)qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0

rungdb: all
	$(call say,QEMU,GDB)
	$(Q)qemu-system-x86_64 -drive file=ipsos.img,format=raw,if=ide,index=0 -s -S