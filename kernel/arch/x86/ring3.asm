[bits 32]

global ring3_launcher

%define USER_CS 0x1B
%define USER_DS 0x23

ring3_launcher:

    ;
    ; DEBUG: launcher reached
    ;
    mov dword [0xB8000], 0x1F451F52    ; "RE"

    ;
    ; hardcoded known-good values
    ;
    mov ebx, 0x00400000    ; user entry
    mov ecx, 0x00501000    ; user stack top

    mov dword [0xB8004], 0x1F4F1F4B    ; "OK"

    ;
    ; load user data selectors
    ;
    mov ax, USER_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ;
    ; Build iret frame EXACTLY as:
    ;
    ;   SS
    ;   ESP
    ;   EFLAGS
    ;   CS
    ;   EIP
    ;
    ; iretd pops in reverse:
    ;   EIP <- top
    ;   CS
    ;   EFLAGS
    ;   ESP
    ;   SS
    ;

    push dword USER_DS      ; SS
    push ecx                ; ESP

    pushfd                  ; EFLAGS

    push dword USER_CS      ; CS
    push ebx                ; EIP

    ;
    ; enter ring3
    ;
    iretd

.hang:
    jmp .hang