[bits 32]

extern irq_handler

%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp irq_common
%endmacro

IRQ  0, 32
IRQ  1, 33
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common:
    pusha

    xor eax, eax
    mov ax, ds
    push eax

    xor eax, eax
    mov ax, es
    push eax

    xor eax, eax
    mov ax, fs
    push eax

    xor eax, eax
    mov ax, gs
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov gs, ax

    pop eax
    mov fs, ax

    pop eax
    mov es, ax

    pop eax
    mov ds, ax

    popa
    add esp, 8
    iret