; ── GDT ───────────────────────────────────────────────────────────────────────
align 8
gdt_start:
    ; null descriptor
    dq 0

    ; 0x08  kernel code: base=0, limit=4GB, 32-bit, ring0, execute/read
    dw 0xFFFF       ; limit[15:0]
    dw 0x0000       ; base[15:0]
    db 0x00         ; base[23:16]
    db 10011010b    ; P=1 DPL=00 S=1 Type=1010 (code, exec, read)
    db 11001111b    ; G=1 DB=1 L=0 AVL=0 limit[19:16]=1111
    db 0x00         ; base[31:24]

    ; 0x10  kernel data: base=0, limit=4GB, 32-bit, ring0, read/write
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; P=1 DPL=00 S=1 Type=0010 (data, read/write)
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

; ── 32-bit entry ──────────────────────────────────────────────────────────────
[bits 32]
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00180000

    ; write 'PM' to top-left of VGA to confirm protected mode
    mov byte [0xB8000], 'P'
    mov byte [0xB8001], 0x0A   ; green on black
    mov byte [0xB8002], 'M'
    mov byte [0xB8003], 0x0A

.halt:
    cli
    hlt
    jmp .halt