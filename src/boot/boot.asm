[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

KERNEL_LOAD_SEG equ 0x1000


start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    xor ax, ax
    mov ds, ax
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov byte [load_sector], 2
    mov byte [load_head], 0
    mov byte [load_cylinder], 0
    mov word [load_remaining], 1024

.load_kernel:
    mov ah, 0x02
    mov al, 1
    mov ch, [load_cylinder]
    mov cl, [load_sector]
    mov dh, [load_head]
    mov dl, 0x80
    push ds
    push es
    int 0x13
    pop es
    pop ds
    jc disk_read_error

    mov ax, es
    add ax, 0x20
    mov es, ax

    inc byte [load_sector]
    cmp byte [load_sector], 64
    jne .load_count
    mov byte [load_sector], 1
    inc byte [load_head]
    cmp byte [load_head], 16
    jne .load_count
    mov byte [load_head], 0
    inc byte [load_cylinder]

.load_count:
    dec word [load_remaining]
    jnz .load_kernel

load_PM:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp CODE_OFFSET:PModeMain

disk_read_error:
    hlt

load_sector db 2
load_head db 0
load_cylinder db 0
load_remaining dw 1024

gdt_start:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00         

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32]
PModeMain:
    mov ax, DATA_OFFSET
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov gs, ax
    mov esp, 0x01000000
    and esp, 0xFFFFFFF0

    in al, 0x92
    or al, 2
    out 0x92, al

    mov al, 0xFF 

    jmp CODE_OFFSET:0x10000

times 510 - ($ - $$) db 0
dw 0xAA55