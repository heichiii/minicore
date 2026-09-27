CROSS ?= riscv64-unknown-elf-
CC := $(CROSS)gcc
QEMU ?= qemu-system-riscv64
GDB ?= gdb-multiarch
PYTHON ?= python3
QEMU_MACHINE ?= virt
QEMU_MEMORY ?= 256M

TARGET := build/kernel.elf
LINKER_SCRIPT := src/linker/linker.ld
USER_PROGRAMS := init child crash text-fault kernel-fault
USER_C_PROGRAMS := m6-test sh echo cat upper
USER_ELFS := $(addprefix build/,$(addsuffix .user.elf,$(USER_PROGRAMS)))
USER_ELFS += $(addprefix build/,$(addsuffix .user.elf,$(USER_C_PROGRAMS)))
OBJS := build/main.o build/runtime.o build/dtb.o build/qemu-virt.o \
	build/plic.o build/sbi.o build/trap.o build/page_alloc.o build/vm.o \
	build/heap.o build/sync.o build/log.o build/scheduler.o build/m4-test.o \
	build/address_space.o build/vfs.o build/pipe.o build/fd.o build/elf.o build/process.o build/user.o \
	build/initramfs.o build/context-asm.o build/trap-asm.o build/head.o
ARCH := -march=rv64imac_zicsr_zicntr_zifencei -mabi=lp64 -mcmodel=medany
CFLAGS := $(ARCH) -std=c11 -O2 -g -Wall -Wextra -Werror \
	-MMD -MP -ffreestanding -fno-builtin -fno-stack-protector -fno-pie

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

build/heap.o: src/mm/heap.c src/mm/heap.h src/mm/page_alloc.h \
		src/kernel/list.h src/kernel/sync.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/sync.o: src/kernel/sync.c src/kernel/sync.h src/kernel/scheduler.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/log.o: src/kernel/log.c src/kernel/log.h src/kernel/sync.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/scheduler.o: src/kernel/scheduler.c src/kernel/scheduler.h \
		src/kernel/sync.h src/mm/heap.h src/mm/page_alloc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/m4-test.o: src/kernel/m4_test.c src/kernel/m4_test.h \
		src/kernel/scheduler.h src/kernel/sync.h src/mm/heap.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/user.o: src/kernel/user.c src/kernel/user.h src/arch/riscv/csr.h \
		src/arch/riscv/trap.h src/mm/vm.h src/mm/page_alloc.h \
		src/mm/layout.h src/platform/platform.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.user.o: user/%.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/%.user.elf: build/%.user.o user/program.ld
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T user/program.ld $< -o $@

build/%.uc.o: user/%.c user/lib.h src/abi.h
	@mkdir -p build
	$(CC) $(CFLAGS) -msmall-data-limit=0 -c $< -o $@

build/crt.user.o: user/crt.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/runtime.uc.o: src/runtime.c
	@mkdir -p build
	$(CC) $(CFLAGS) -msmall-data-limit=0 -c $< -o $@

$(addprefix build/,$(addsuffix .user.elf,$(USER_C_PROGRAMS))): build/%.user.elf: build/%.uc.o build/lib.uc.o build/runtime.uc.o build/crt.user.o user/program.ld
	$(CC) $(ARCH) -nostdlib -static -no-pie -Wl,--build-id=none \
		-T user/program.ld build/crt.user.o $< build/lib.uc.o build/runtime.uc.o -lgcc -o $@

build/initramfs.cpio: $(USER_ELFS) user/bad.bin tools/mkinitramfs.py
	$(PYTHON) tools/mkinitramfs.py $@ \
		init=build/init.user.elf bin/child=build/child.user.elf \
		bin/crash=build/crash.user.elf \
		bin/text-fault=build/text-fault.user.elf \
		bin/kernel-fault=build/kernel-fault.user.elf \
		bin/bad=user/bad.bin \
		$(foreach name,$(USER_C_PROGRAMS),bin/$(name)=build/$(name).user.elf)

build/initramfs.o: build/initramfs.cpio
	$(CROSS)objcopy -I binary -O elf64-littleriscv -B riscv \
		--rename-section .data=.rodata.initramfs,alloc,load,readonly,data,contents \
		$< $@

build/trap-asm.o: src/arch/riscv/trap.S
	@mkdir -p build
	$(CC) $(ARCH) -g -c $< -o $@

build/context-asm.o: src/arch/riscv/context.S
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
		timeout 30s $(QEMU) -machine virt -m $$memory -smp 1 -nographic \
			-bios default -kernel $(TARGET) >$$log 2>&1; \
		grep -q "M4 PASS" $$log; \
		grep -q "M5 PASS" $$log; \
		grep -q "M6 USER PASS" $$log; \
		grep -q "M6 PASS" $$log; \
		echo "M1-M6 $$memory PASS"; \
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

build/address_space.o: src/mm/address_space.c src/mm/address_space.h src/mm/vm.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/process.o: src/kernel/process.c src/kernel/process.h src/kernel/scheduler.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/vfs.o: src/fs/vfs.c src/fs/vfs.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/elf.o: src/kernel/elf.c src/kernel/elf.h src/fs/vfs.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

-include $(wildcard build/*.d)

build/pipe.o: src/fs/pipe.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/fd.o: src/kernel/fd.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
