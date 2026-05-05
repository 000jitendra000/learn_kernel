[bits 32]

global kernel_entry
extern kernel_main

kernel_entry:
    mov esp, 0x00180000
    mov ebp, 0
    cld
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt