ASM := nasm
CC  := gcc
LD  := ld
USER_DIR := users

SHELL_ELF := build/shell.elf
HELLO_ELF := build/hello.elf

CFLAGS  := -m32 -ffreestanding -fno-stack-protector -fno-pie -fno-pic \
           -nostdlib -nostdinc -Wall -Wextra -O2 \
           -I include
LDFLAGS := -m elf_i386 -T linker/linker.ld

BOOT_BIN   := build/boot.bin
KERNEL_ELF := build/kernel.elf
KERNEL_BIN := build/kernel.bin
DISK_IMG   := disk/os.img

KERNEL_OBJS := \
    build/entry.o \
    build/kernel.o \
    build/vga.o \
    build/string.o \
    build/idt.o \
    build/idt_asm.o \
    build/isr.o \
    build/irq.o \
    build/keyboard.o \
    build/timer.o \
    build/pmm.o \
    build/paging.o \
    build/heap.o \
    build/task.o \
    build/scheduler.o \
    build/switch.o \
    build/gdt.o \
    build/gdt_asm.o \
    build/ring3.o \
    build/syscall.o \
    build/process.o \
    build/elf.o \
    build/fs.o \
    build/kbd_buf.o \
	build/shell_bin.o \
	build/hello_bin.o \
    build/pipe.o

.PHONY: all clean run debug

all: $(DISK_IMG)

$(BOOT_BIN): boot/boot.asm
	@mkdir -p build
	$(ASM) -f bin -o $@ $<

build/entry.o: kernel/entry.asm
	@mkdir -p build
	$(ASM) -f elf32 -o $@ $<

build/idt_asm.o: kernel/arch/x86/idt.asm
	$(ASM) -f elf32 -o $@ $<

build/isr.o: kernel/arch/x86/isr.asm
	$(ASM) -f elf32 -o $@ $<

build/irq.o: kernel/arch/x86/irq.asm
	$(ASM) -f elf32 -o $@ $<

build/switch.o: kernel/arch/x86/switch.asm
	$(ASM) -f elf32 -o $@ $<

build/gdt_asm.o: kernel/arch/x86/gdt.asm
	$(ASM) -f elf32 -o $@ $<

build/ring3.o: kernel/arch/x86/ring3.asm
	$(ASM) -f elf32 -o $@ $<

build/gdt.o: kernel/arch/x86/gdt.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/vga.o: kernel/drivers/vga.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/string.o: kernel/lib/string.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/idt.o: kernel/arch/x86/idt.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/keyboard.o: kernel/drivers/keyboard.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/timer.o: kernel/drivers/timer.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/pmm.o: kernel/mm/pmm.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/paging.o: kernel/arch/x86/paging.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/heap.o: kernel/mm/heap.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/task.o: kernel/sched/task.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/scheduler.o: kernel/sched/scheduler.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/syscall.o: kernel/arch/x86/syscall.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/process.o: kernel/proc/process.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/elf.o: kernel/exec/elf.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/fs.o: kernel/fs/fs.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/kbd_buf.o: kernel/drivers/kbd_buf.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/pipe.o: kernel/ipc/pipe.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/shell_bin.o: $(SHELL_ELF)
	$(LD) -m elf_i386 -r -b binary -o $@ $<

build/hello_bin.o: $(HELLO_ELF)
	$(LD) -m elf_i386 -r -b binary -o $@ $<

$(KERNEL_ELF): $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(SHELL_ELF): $(USER_DIR)/shell.asm
	$(ASM) -f elf32 -o build/shell.o $<
	$(LD) -m elf_i386 -N -e _start -Ttext 0x00400000 -o $@ build/shell.o

$(HELLO_ELF): $(USER_DIR)/hello.asm
	$(ASM) -f elf32 -o build/hello.o $<
	$(LD) -m elf_i386 -N -e _start -Ttext 0x00400000 -o $@ build/hello.o

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

$(DISK_IMG): $(BOOT_BIN) $(KERNEL_BIN) $(SHELL_ELF) $(HELLO_ELF)
	@mkdir -p disk
	dd if=/dev/zero     of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN)   of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

clean:
	rm -rf build disk

run: $(DISK_IMG)
	qemu-system-i386 \
	    -drive file=$(DISK_IMG),format=raw,index=0,media=disk \
	    -m 32M -no-reboot -no-shutdown

debug: $(DISK_IMG)
	qemu-system-i386 \
	    -drive file=$(DISK_IMG),format=raw,index=0,media=disk \
	    -m 32M -no-reboot -no-shutdown -s -S &
	gdb build/kernel.elf \
	    -ex "target remote localhost:1234" \
	    -ex "set architecture i386" \
	    -ex "break kernel_main" \
	    -ex "continue"