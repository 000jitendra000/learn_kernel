[bits 32]

global gdt_load
global tss_load

gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]
    jmp 0x08:.flush
.flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

tss_load:
    mov ax, 0x28
    ltr ax
    ret