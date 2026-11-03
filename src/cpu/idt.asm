[BITS 32]

global idt_load
global isr_stub
global irq_stub
global isr6
global irq1_stub

extern keyboard_handler

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