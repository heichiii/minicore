CROSS ?= riscv64-unknown-elf-
CC := $(CROSS)gcc
QEMU ?= qemu-system-riscv64
GDB ?= gdb-multiarch
PYTHON ?= python3
QEMU_MACHINE ?= virt
QEMU_MEMORY ?= 256M

TARGET := build/kernel.elf
LINKER_SCRIPT := src/linker/linker.ld
OBJS := build/main.o build/runtime.o build/dtb.o build/qemu-virt.o build/head.o
ARCH := -march=rv64imac -mabi=lp64 -mcmodel=medany
CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pie

.PHONY: all run test debug gdb layout clean
all: $(TARGET)

$(TARGET): $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T $(LINKER_SCRIPT) $(OBJS) -lgcc -o $@

build/main.o: src/main.c src/platform/dtb.h src/platform/platform.h src/runtime.h
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

build/head.o: src/arch/riscv/head.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

run: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET)

test: $(TARGET)
	@timeout 10s $(QEMU) -machine virt -m 32M -smp 1 -nographic \
		-bios default -kernel $(TARGET) | tee build/test-32m.log
	@grep -q 'ram: base=0x0000000080000000 size=0x0000000002000000' build/test-32m.log
	@grep -q 'M1 DTB PASS' build/test-32m.log
	@timeout 10s $(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET) | tee build/test-256m.log
	@grep -q 'ram: base=0x0000000080000000 size=0x0000000010000000' build/test-256m.log
	@grep -q 'M1 DTB PASS' build/test-256m.log

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
