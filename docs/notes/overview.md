*本项目是一个跑在qemu riscv64 virt的mini os.*

# 外部条件

qemu virt提供了许多MMIO.RAM起始地址是0x8000 0000.

qemu完成了opensbi的加载运行，opensbi把控制流交给0x8020 0000. qemu也完成我们kernel程序的加载。

# 进入OS

## 如何进入？

opensbi执行完后会跳转到0x8020 0000, 如何确保我们的程序被qemu正确加载到这个地址？

qemu会根据elf中段的物理地址将其加载到对应位置，所以我们需要在链接器脚本中设置好地址从而让elf中对应段的物理地址是0x8020 0000.

## 初始内存布局

在执行第一条指令前，我们有必要知道当前的内存布局。

```
celestee@Celestee-debian:~/prj/minicore$ readelf -l build/kernel.elf 

Elf file type is EXEC (Executable file)
Entry point 0x80200000
There are 5 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000001000 0x0000000080200000 0x0000000080200000
                 0x00000000000000b2 0x00000000000000b2  R E    0x1000
  LOAD           0x0000000000000000 0x0000000080201000 0x0000000080201000
                 0x0000000000000000 0x0000000000001000  RW     0x1000
  LOAD           0x0000000000002000 0xffffffc080202000 0x0000000080202000
                 0x00000000000325a7 0x00000000000325a7  R E    0x1000
  LOAD           0x0000000000035000 0xffffffc080235000 0x0000000080235000
                 0x0000000000000020 0x0000000000006000  RW     0x1000
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x10

 Section to Segment mapping:
  Segment Sections...
   00     .text.boot 
   01     .bss.boot 
   02     .text .rodata .srodata.cst8 .srodata 
   03     .data .bss 
   04    
```

通过readelf我们可以看到，四个LOAD段会被加载。

1. .text.boot节只包含最初的启动程序。
2. .bss.boot节只包含存储启动临时页表的4KiB字节内存。
3. .text .rodata .srodata.cst8 .srodata 是编译器生成的普通代码使用的只读区域。
4. .data .bss 是编译器生成的普通代码使用的读写区域。其中，链接器脚本还在.bss末尾预留了12K的启动栈空间。

## 执行启动准备

head.S中的`_start`被链接器脚本标记为入口，因此最先执行。`_start`先把opensbi传来的hart ID参数和DTB地址参数从a0 a1寄存器转移到s0 s1寄存器（因为按照 RISC-V 调用约定，`a0-a7` 是**参数/返回值寄存器，也是 caller-saved 寄存器**，后续一旦调用函数，它们很可能被覆盖。而 `s0-s11` 是**callee-saved 寄存器**，被调用函数必须保证返回时恢复原值，所以更适合长期保存启动参数）。

清空临时页表所在的内存。在页表内存直接写页表项（直接使用1GiB的大页），把起始的4GiB内存分别恒等映射到原位置和映射到加0xffffffc000000000后的高地址。写完页表后启用MMU,使用刚写好的临时页表。跳转到.text节的高地址代码（链接器脚本把普通节放在高地址，从readelf也能看到）。

把sp指针设为启动栈顶。

清空bss内存。

然后使用hart ID参数和DTB地址参数调用kernel_main.

# kernel main

进入kernel main就进入了C语言。



