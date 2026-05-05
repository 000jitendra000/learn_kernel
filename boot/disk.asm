; ── disk.asm ──────────────────────────────────────────────────────────────────
; Called from boot.asm BEFORE protected mode switch (still in real mode).
; Loads KERNEL_SECTORS sectors starting at sector 2 directly into 0x00100000.
;
; Limitation: BIOS INT 13h / AH=02h uses a seg:off address.
; 0x00100000 is above the 1MB real-mode limit.
; Solution: use the BIOS "unreal mode" trick — load a 32-bit data segment
; into FS while still in real mode (CPU allows 32-bit addressing once the
; descriptor cache is primed), then write through FS.
;
; Steps:
;   1. enter unreal mode (load GDT, set CR0.PE, reload FS, clear CR0.PE)
;   2. INT 13h extended read into a below-1MB bounce buffer (0x9000)
;   3. copy from 0x9000 into FS:0x00100000 using 32-bit rep movsd
;   4. repeat for each sector group

KERNEL_START_SECTOR equ 2          ; sector index (0-based LBA... actually CHS below)
KERNEL_SECTORS      equ 64         ; 64 × 512 = 32 KB max kernel
BOUNCE_SEG          equ 0x0900     ; bounce buffer at 0x9000 (phys)
BOUNCE_OFF          equ 0x0000
SECTORS_PER_CHUNK   equ 8          ; read 8 sectors (4 KB) at a time

[bits 16]
load_kernel:
    pusha

    ; ── 1. enter unreal mode to get a 4GB FS descriptor ──────────────────────
    cli
    lgdt [gdt_descriptor]          ; gdt_descriptor defined in protected_mode.asm

    mov eax, cr0
    or  eax, 0x1
    mov cr0, eax

    ; load FS with the 32-bit flat data descriptor (0x10)
    mov bx, 0x10
    mov fs, bx

    ; back to real mode
    and eax, ~0x1
    mov cr0, eax
    jmp 0x0000:.unreal_ok

.unreal_ok:
    sti

    ; ── 2. load sectors in chunks via INT 13h ─────────────────────────────────
    xor edi, edi                   ; dest offset into 0x00100000 (starts at 0)
    mov cl, KERNEL_START_SECTOR    ; current CHS sector (1-based for INT 13h: sector 1 = first)
    ; Note: CHS sector numbers are 1-based. Sector 0 = boot. Sector 1 = first after boot.
    ; Our disk layout: kernel at sector 2 onward (LBA 1 = CHS sector 2... depends on BIOS).
    ; Using LBA via CHS mapping on single-track image: cyl=0, head=0, sector=CL (1-based).
    ; Sector 2 (LBA 1) → CHS 0/0/2.
    mov cl, 2                      ; CHS sector 2 = LBA 1 (first kernel sector)
    mov si, 0                      ; sectors loaded so far

.load_loop:
    cmp si, KERNEL_SECTORS
    jge .load_done

    ; read up to SECTORS_PER_CHUNK sectors into bounce buffer
    mov ax, BOUNCE_SEG
    mov es, ax
    xor bx, bx                     ; ES:BX = 0x9000:0x0000

    mov ah, 0x02                   ; INT 13h read
    mov al, SECTORS_PER_CHUNK      ; sectors to read
    mov ch, 0                      ; cylinder 0
    mov dh, 0                      ; head 0
    ; cl already = current sector
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error

    ; copy SECTORS_PER_CHUNK × 512 bytes from 0x9000 into FS:0x100000+edi
    push esi
    mov  esi, 0x9000               ; source: linear 0x9000
    mov  ecx, (SECTORS_PER_CHUNK * 512) / 4

.copy_loop:
    mov  eax, [ds:esi]             ; read dword from bounce (ds=0, so linear = esi)
    mov  fs:[0x00100000 + edi], eax
    add  esi, 4
    add  edi, 4
    dec  ecx
    jnz  .copy_loop

    pop  esi

    add  cl, SECTORS_PER_CHUNK     ; advance CHS sector
    add  si, SECTORS_PER_CHUNK     ; sectors loaded count
    jmp  .load_loop

.load_done:
    popa
    ret

.disk_error:
    mov si, msg_disk_err
    cld
    call print                     ; print defined in boot.asm
    cli
    hlt

msg_disk_err db 'Disk error!', 0x0D, 0x0A, 0