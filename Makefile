# Makefile — NovaOS build automation
TARGET   := x86_64-elf
CC       := $(TARGET)-gcc
LD       := $(TARGET)-ld
AS       := nasm
CFLAGS   := -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
            -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -mno-avx \
            -Wall -Wextra -O2 -c
ASFLAGS  := -f elf64
LDFLAGS  := -T linker.ld -nostdlib
BUILD_DIR := build
ISO_DIR   := iso
OBJS := $(BUILD_DIR)/boot.o $(BUILD_DIR)/long_mode_init.o \
        $(BUILD_DIR)/kernel.o $(BUILD_DIR)/idt.o $(BUILD_DIR)/isr.o \
        $(BUILD_DIR)/pic.o $(BUILD_DIR)/pit.o $(BUILD_DIR)/keyboard.o \
        $(BUILD_DIR)/irq.o $(BUILD_DIR)/irq_asm.o $(BUILD_DIR)/pmm.o \
        $(BUILD_DIR)/heap.o $(BUILD_DIR)/slab.o \
        $(BUILD_DIR)/scheduler.o $(BUILD_DIR)/context_switch.o
.PHONY: all clean run iso
all: $(BUILD_DIR)/kernel.elf
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(BUILD_DIR)/boot.o: boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) boot.asm -o $@
$(BUILD_DIR)/long_mode_init.o: long_mode_init.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) long_mode_init.asm -o $@
$(BUILD_DIR)/isr.o: isr.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) isr.asm -o $@
$(BUILD_DIR)/irq_asm.o: irq.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) irq.asm -o $@
$(BUILD_DIR)/context_switch.o: context_switch.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) context_switch.asm -o $@
$(BUILD_DIR)/kernel.o: kernel.c idt.h irq.h pmm.h multiboot2.h heap.h slab.h scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) kernel.c -o $@
$(BUILD_DIR)/idt.o: idt.c idt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) idt.c -o $@
$(BUILD_DIR)/pic.o: pic.c pic.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) pic.c -o $@
$(BUILD_DIR)/pit.o: pit.c pit.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) pit.c -o $@
$(BUILD_DIR)/keyboard.o: keyboard.c keyboard.h io.h pic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) keyboard.c -o $@
$(BUILD_DIR)/irq.o: irq.c irq.h pic.h pit.h keyboard.h scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) irq.c -o $@
$(BUILD_DIR)/pmm.o: pmm.c pmm.h multiboot2.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) pmm.c -o $@
$(BUILD_DIR)/heap.o: heap.c heap.h pmm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) heap.c -o $@
$(BUILD_DIR)/slab.o: slab.c slab.h pmm.h heap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) slab.c -o $@
$(BUILD_DIR)/scheduler.o: scheduler.c scheduler.h heap.h pmm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) scheduler.c -o $@
$(BUILD_DIR)/kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
iso: $(BUILD_DIR)/kernel.elf
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/boot/kernel.elf
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o novaos.iso $(ISO_DIR)
run: iso
	qemu-system-x86_64 -cdrom novaos.iso -m 512M -serial stdio
clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) novaos.iso
