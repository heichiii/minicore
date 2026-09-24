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
	build/plic.o build/sbi.o build/trap.o build/page_alloc.o build/vm.o \
	build/user.o build/user-image.o build/trap-asm.o build/head.o
ARCH := -march=rv64imac_zicsr_zicntr -mabi=lp64 -mcmodel=medany
CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-pie

.PHONY: all run test debug gdb layout clean
all: $(TARGET)

$(TARGET): $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T $(LINKER_SCRIPT) $(OBJS) -lgcc -o $@

build/main.o: src/main.c src/arch/riscv/sbi.h src/arch/riscv/trap.h \
		src/kernel/user.h src/platform/dtb.h src/platform/plic.h \
		src/platform/platform.h src/runtime.h
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
		src/arch/riscv/csr.h src/arch/riscv/sbi.h src/kernel/user.h \
		src/platform/platform.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/page_alloc.o: src/mm/page_alloc.c src/mm/page_alloc.h src/mm/layout.h \
		src/platform/dtb.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/vm.o: src/mm/vm.c src/mm/vm.h src/mm/page_alloc.h src/mm/layout.h \
		src/platform/dtb.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/user.o: src/kernel/user.c src/kernel/user.h src/arch/riscv/csr.h \
		src/arch/riscv/trap.h src/mm/vm.h src/mm/page_alloc.h \
		src/mm/layout.h src/platform/platform.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/user-program.o: user/user.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/user.elf: build/user-program.o user/user.ld
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T user/user.ld build/user-program.o -o $@

build/user.bin: build/user.elf
	$(CROSS)objcopy -O binary $< $@

build/user-image.o: build/user.bin
	$(CROSS)objcopy -I binary -O elf64-littleriscv -B riscv \
		--rename-section .data=.rodata.user,alloc,load,readonly,data,contents \
		$< $@

build/trap-asm.o: src/arch/riscv/trap.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/head.o: src/arch/riscv/head.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

run: $(TARGET)
	$(QEMU) -machine virt -m 256M -smp 1 -nographic \
		-bios default -kernel $(TARGET)

test: $(TARGET)
	@set -e; for memory in 256M 32M; do \
		log=build/test-$$memory.log; \
		timeout 20s $(QEMU) -machine virt -m $$memory -smp 1 -nographic \
			-bios default -kernel $(TARGET) >$$log 2>&1; \
		grep -q "M3 PASS" $$log; \
		echo "M3 $$memory PASS"; \
	done

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
