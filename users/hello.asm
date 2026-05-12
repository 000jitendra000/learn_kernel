BITS 32
GLOBAL _start

%define SYS_WRITE 0
%define SYS_EXIT  3

SECTION .data
msg db 'HELLO FROM RING3',10,0

SECTION .text

_start:

    mov eax, SYS_WRITE
    mov ebx, msg
    int 0x80

    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80

.loop:
    jmp .loop