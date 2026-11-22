[BITS 32]

global idt_load
global isr_stub
global irq_stub
global isr6
global irq1_stub
global irq0_stub
global task_yield_stub
global irq12_stub

extern mouse_handler
extern keyboard_handler
extern task_schedule


irq0_stub:
    push ds
    push es
    push fs
    push gs
    pushad
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    mov al, 0x20
    out 0x20, al
    mov eax, esp
    push dword 1
    push eax
    call task_schedule
    add esp, 8
    mov esp, eax
    popad
    pop gs
    pop fs
    pop es
    pop ds
    iretd

task_yield_stub:
    push ds
    push es
    push fs
    push gs
    pushad
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    mov eax, esp
    push dword 0
    push eax
    call task_schedule
    add esp, 8
    mov esp, eax
    popad
    pop gs
    pop fs
    pop es
    pop ds
    iretd

irq1_stub:
    pushad
    call keyboard_handler
    mov al, 0x20
    out 0x20, al
    popad
    iret

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

irq_stub:
    pushad
    mov al, 0x20
    out 0x20, al
    popad
    iret

isr6:
    cli
    hlt

isr_stub:
    iret

irq12_stub:
    pushad
    call mouse_handler

    mov al, 0x20
    out 0xA0, al

    mov al, 0x20
    out 0x20, al

    popad
    iret