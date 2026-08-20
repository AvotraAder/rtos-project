# ── Makefile - RTOS x86 v3.0 ─────────────────────────────────────
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
OBJS =  $(BUILD)/boot.o           \
        $(BUILD)/gdt_flush.o      \
        $(BUILD)/interrupts.o     \
        $(BUILD)/gdt.o            \
        $(BUILD)/idt.o            \
        $(BUILD)/timer.o          \
        $(BUILD)/task.o           \
        $(BUILD)/process.o        \
        $(BUILD)/keyboard.o       \
        $(BUILD)/mutex.o          \
        $(BUILD)/semaphore.o      \
        $(BUILD)/queue.o          \
        $(BUILD)/heap.o           \
        $(BUILD)/vfs.o            \
        $(BUILD)/syscall.o        \
        $(BUILD)/shell.o          \
        $(BUILD)/kernel.o         \
        $(BUILD)/panic.o          \
        $(BUILD)/error.o          \
        $(BUILD)/logging.o        \
        $(BUILD)/paging.o         \
        $(BUILD)/signal.o         \
        $(BUILD)/pipe.o           \
        $(BUILD)/ring.o           \
        $(BUILD)/serial.o

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
	@echo "  ║  RTOS v3.0 kernel.bin compilé   ║"
	@echo "  ║  Avec: Logging, Paging,         ║"
	@echo "  ║        Signaux, Rings, IPC      ║"
	@echo "  ╚══════════════════════════════════╝"
	@echo ""

# ── Exécution ─────────────────────────────────────────────────────
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -serial stdio \
	    -m 32M \
	    -d guest_errors

run-debug: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) \
	    -serial stdio \
	    -m 32M \
	    -gdb tcp::1234 \
	    -S

# ── ISO GRUB ──────────────────────────────────────────────────────
iso: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\n\nmenuentry "RTOS v3.0" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' \
	    > $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

# ── Nettoyage ─────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD) $(ISO_DIR) $(ISO)

# ── Aide ───────────────────────────────────────────────────────────
help:
	@echo "RTOS x86 v3.0 - Cibles disponibles:"
	@echo "  make all       - Compiler le noyau (par défaut)"
	@echo "  make run       - Exécuter dans QEMU"
	@echo "  make run-debug - Exécuter en debug (gdb :1234)"
	@echo "  make iso       - Créer une ISO GRUB"
	@echo "  make run-iso   - Exécuter l'ISO dans QEMU"
	@echo "  make clean     - Nettoyer les fichiers compilés"
