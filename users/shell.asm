BITS 32
GLOBAL _start

%define SYS_WRITE 0
%define SYS_EXIT  3
%define SYS_READ  5
%define SYS_EXEC  9
%define SYS_WAIT  10

SECTION .data

help_cmd db 'help',0

helpmsg db 'commands: help hello exit',10,0

dbgbuf db '[BUF]=',0
newline db 10,0

bootmsg db '[shell started]',10,0

prompt      db '$ ',0
execmsg     db '[exec]',10,0
aftermsg    db '[after]',10,0
failmsg     db 'exec failed',10,0

hello_cmd   db 'hello',0
exit_cmd    db 'exit',0

SECTION .bss

buf resb 128

SECTION .text

_start:
    mov eax, SYS_WRITE
    mov ebx, bootmsg
    int 0x80

.loop:

    ; print prompt
    mov eax, SYS_WRITE
    mov ebx, prompt
    int 0x80

    ; read line
    mov eax, SYS_READ
    mov ebx, 0
    mov ecx, buf
    mov edx, 127
    int 0x80

    cmp eax, 0
    jle .loop

    ; save returned length
    mov esi, eax

    ; debug print buffer
    mov eax, SYS_WRITE
    mov ebx, dbgbuf
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, buf
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, newline
    int 0x80

    ; null terminate
    mov byte [buf + esi], 0

    ; empty?
    cmp byte [buf], 0
    je .loop

    ; exit?
    mov esi, buf
    mov edi, exit_cmd
    call strcmp

    cmp eax, 1
    je .exit

    ; help?
    mov esi, buf
    mov edi, help_cmd
    call strcmp

    cmp eax, 1
    jne .do_exec

    ; built-in hello debug
    mov eax, SYS_WRITE
    mov ebx, helpmsg
    int 0x80

    jmp .loop

.do_exec:

    ; DEBUG
    mov eax, SYS_WRITE
    mov ebx, execmsg
    int 0x80

    ; exec command
    mov eax, SYS_EXEC
    mov ebx, buf
    int 0x80

    ; save child pid
    mov esi, eax

    ; DEBUG
    mov eax, SYS_WRITE
    mov ebx, aftermsg
    int 0x80

    cmp esi, 0
    jl .fail

    ; wait child
    mov ebx, esi
    mov eax, SYS_WAIT
    int 0x80

    jmp .loop

.fail:
    mov eax, SYS_WRITE
    mov ebx, failmsg
    int 0x80
    jmp .loop

.exit:
    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80

.hang:
    jmp .hang

strcmp:

.loop:
    mov al, [esi]
    cmp al, [edi]
    jne .no

    test al, al
    je .yes

    inc esi
    inc edi
    jmp .loop

.yes:
    mov eax, 1
    ret

.no:
    xor eax, eax
    ret
.help:
    mov eax, SYS_WRITE
    mov ebx, helpmsg
    int 0x80
    jmp .loop