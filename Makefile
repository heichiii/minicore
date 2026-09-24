CROSS ?= riscv64-unknown-elf-
CC := $(CROSS)gcc
QEMU ?= qemu-system-riscv64
GDB ?= gdb-multiarch
PYTHON ?= python3
QEMU_MACHINE ?= virt
QEMU_MEMORY ?= 256M

TARGET := build/kernel.elf
LINKER_SCRIPT := src/linker/linker.ld
OBJS := build/main.o build/runtime.o build/dtb.o build/qemu-virt.o \
	build/plic.o build/sbi.o build/trap.o build/trap-asm.o build/head.o
ARCH := -march=rv64imac_zicsr_zicntr -mabi=lp64 -mcmodel=medany
CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pie

.PHONY: all run debug gdb layout clean
all: $(TARGET)

$(TARGET): $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T $(LINKER_SCRIPT) $(OBJS) -lgcc -o $@

build/main.o: src/main.c src/arch/riscv/sbi.h src/arch/riscv/trap.h \
		src/platform/dtb.h src/platform/plic.h src/platform/platform.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/runtime.o: src/runtime.c src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/dtb.o: src/platform/dtb.c src/platform/dtb.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/qemu-virt.o: src/platform/qemu-virt.c src/platform/platform.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/plic.o: src/platform/plic.c src/platform/plic.h src/platform/dtb.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/sbi.o: src/arch/riscv/sbi.c src/arch/riscv/sbi.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/trap.o: src/arch/riscv/trap.c src/arch/riscv/trap.h \
		src/arch/riscv/csr.h src/arch/riscv/sbi.h src/platform/platform.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/trap-asm.o: src/arch/riscv/trap.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/head.o: src/arch/riscv/head.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

run: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET)

debug: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET) -S -s

gdb: $(TARGET)
	$(GDB) $(TARGET) -ex 'target remote :1234'

layout: $(TARGET)
	$(PYTHON) tools/generate-layout.py \
		--elf $(TARGET) \
		--readelf $(CROSS)readelf \
		--nm $(CROSS)nm \
		--qemu $(QEMU) \
		--machine $(QEMU_MACHINE) \
		--memory $(QEMU_MEMORY) \
		--output build/layout.html

clean:
	rm -rf build
