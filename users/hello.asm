BITS 32
GLOBAL _start

SECTION .text

_start:

.loop:

    ; syscall 2 = getchar
    mov eax, 2
    int 0x80

    cmp eax, 0
    je .loop

    ; print returned char
    mov ebx, eax
    mov eax, 1
    int 0x80

    jmp .loop