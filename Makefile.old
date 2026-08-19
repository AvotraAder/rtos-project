# ── Makefile - RTOS x86 v2.0 ─────────────────────────────────────
CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-gcc

CFLAGS  = -std=gnu11 -ffreestanding -O2 -Wall -Wextra \
          -Wno-unused-parameter -fno-stack-protector
LDFLAGS = -ffreestanding -O2 -nostdlib -lgcc

BUILD   = build
ISO_DIR = isodir
KERNEL  = $(BUILD)/kernel.bin
ISO     = rtos.iso

# ── Objets ────────────────────────────────────────────────────────
OBJS =  $(BUILD)/boot.o        \
        $(BUILD)/gdt_flush.o   \
        $(BUILD)/interrupts.o  \
        $(BUILD)/gdt.o         \
        $(BUILD)/idt.o         \
        $(BUILD)/timer.o       \
        $(BUILD)/task.o        \
        $(BUILD)/process.o     \
        $(BUILD)/keyboard.o    \
        $(BUILD)/mutex.o       \
        $(BUILD)/semaphore.o   \
        $(BUILD)/queue.o       \
        $(BUILD)/heap.o        \
        $(BUILD)/vfs.o         \
        $(BUILD)/syscall.o     \
        $(BUILD)/shell.o       \
        $(BUILD)/kernel.o

.PHONY: all clean run iso run-iso

# ── Cibles principales ────────────────────────────────────────────
all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

# ── Assembleur ────────────────────────────────────────────────────
$(BUILD)/boot.o: boot.s | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/gdt_flush.o: gdt_flush.s | $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/interrupts.o: interrupts.s | $(BUILD)
	$(AS) -f elf32 $< -o $@

# ── C ─────────────────────────────────────────────────────────────
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Linkage ───────────────────────────────────────────────────────
$(KERNEL): $(OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(LDFLAGS) $(OBJS)
	@echo ""
	@echo "  ╔══════════════════════════════════╗"
	@echo "  ║  RTOS kernel.bin compilé OK !    ║"
	@echo "  ╚══════════════════════════════════╝"
	@echo ""

# ── Exécution ─────────────────────────────────────────────────────
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -serial stdio \
	    -m 32M

# ── ISO GRUB ──────────────────────────────────────────────────────
iso: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\n\nmenuentry "RTOS v2.0" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' \
	    > $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

# ── Nettoyage ─────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD) $(ISO_DIR) $(ISO)
