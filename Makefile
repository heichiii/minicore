CROSS ?= riscv64-unknown-elf-
CC := $(CROSS)gcc
QEMU ?= qemu-system-riscv64
GDB ?= gdb-multiarch

TARGET := build/kernel.elf
OBJS := build/main.o build/runtime.o build/qemu-virt.o build/head.o
ARCH := -march=rv64imac -mabi=lp64 -mcmodel=medany
CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pie

.PHONY: all run test debug gdb clean
all: $(TARGET)

$(TARGET): $(OBJS) src/arch/riscv/linker.ld
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T src/arch/riscv/linker.ld $(OBJS) -lgcc -o $@

build/main.o: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/runtime.o: src/runtime.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/qemu-virt.o: src/platform/qemu-virt.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/head.o: src/arch/riscv/head.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

run: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET)

test: $(TARGET)
	@timeout 10s $(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET) | tee build/test.log
	@grep -q 'M0 PASS' build/test.log

debug: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET) -S -s

gdb: $(TARGET)
	$(GDB) $(TARGET) -ex 'target remote :1234'

clean:
	rm -rf build
