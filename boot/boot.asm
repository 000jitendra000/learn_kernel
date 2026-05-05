[bits 16]
[org 0x7C00]

KERNEL_SECTORS equ 64
STAGE_SEG      equ 0x1000
CODE_SEG       equ 0x08
DATA_SEG       equ 0x10

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000
    sti

    mov [boot_drive], dl

    mov si, msg
    cld
    call print

    ; load kernel into staging buffer at 0x10000
    mov ax, STAGE_SEG
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0
    mov dh, 0
    mov cl, 2
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; enable A20
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE
    out 0x92, al

    ; protected mode
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:pm_entry

disk_error:
    mov si, msg_err
    cld
    call print
    cli
    hlt

print:
    mov ah, 0x0E

.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop

.done:
    ret

msg db 'MyKernel booting...', 0x0D, 0x0A, 0
msg_err db 'Disk error', 0x0D, 0x0A, 0
boot_drive db 0

align 8
gdt_start:
    dq 0

    ; code segment
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

    ; data segment
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

[bits 32]
pm_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00180000

    ; copy kernel from 0x10000 to 0x100000
    mov esi, 0x00010000
    mov edi, 0x00100000
    mov ecx, (KERNEL_SECTORS * 512) / 4
    cld
    rep movsd

    jmp CODE_SEG:0x00100000

times 510 - ($ - $$) db 0
dw 0xAA55