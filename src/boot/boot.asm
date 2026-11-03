[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

KERNEL_LOAD_SEG equ 0x1000

VBE_INFO_ADDR   equ 0x8000
MODE_INFO_ADDR  equ 0x8200

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    call set_video_mode
    
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 64
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x80
    int 0x13
    jc disk_read_error

load_PM:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp CODE_OFFSET:PModeMain

disk_read_error:
    hlt

set_video_mode:
    mov ax, 0x4F00
    mov di, 0x8000         
    int 0x10
    cmp ax, 0x004F
    jne .vbe_error

    mov si, [0x8000 + 0x0E]
    mov ax, [0x8000 + 0x10] 
    mov ds, ax
    mov si, [0x8000 + 0x0E] 

.find_mode_loop:
    mov cx, [ds:si]         
    cmp cx, 0xFFFF          
    je .vbe_error

    push si              
    mov ax, 0x4F01
    mov di, 0x8200          
    int 0x10
    pop si
    
    cmp ax, 0x004F
    jne .next_mode

    mov ax, [0x8200 + 0x12] 
    cmp ax, 1024
    jne .next_mode

    mov ax, [0x8200 + 0x14]
    cmp ax, 768
    jne .next_mode

    mov al, [0x8200 + 0x19]
    cmp al, 32
    jne .next_mode

    mov [0x900A], cx       

    mov ax, 0x4F02
    mov bx, cx
    or bx, 0x4000           
    int 0x10
    cmp ax, 0x004F
    jne .vbe_error

    mov eax, [0x8200 + 0x28]
    mov [0x9000], eax       

    mov ax, [0x8200 + 0x10] 
    mov [0x9004], ax

    mov ax, [0x8200 + 0x12] 
    mov [0x9006], ax

    mov ax, [0x8200 + 0x14] 
    mov [0x9008], ax

    ret

.next_mode:
    add si, 2           
    jmp .find_mode_loop

.vbe_error:
    mov dword [0x9000], 0xDEADBEEF
    ret
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
    mov esp, 0x90000
    and esp, 0xFFFFFFF0

    in al, 0x92
    or al, 2
    out 0x92, al

    mov al, 0xFF 

    jmp CODE_OFFSET:0x10000

times 510 - ($ - $$) db 0
dw 0xAA55