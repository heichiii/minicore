    .section .text.boot, "ax", @progbits
    .align 2
    .globl _start
    .type _start, @function
_start:
    /* OpenSBI 在 MMU 关闭（satp.MODE=Bare）的状态下跳到这里：
     *   a0 = 当前 hart ID
     *   a1 = DTB 的物理地址
     * 此时还没有可用栈，因此先用被调用者保存寄存器保留启动参数。 */
    mv s0, a0
    mv s1, a1

    /* boot_root_page 是一个 4 KiB 的 Sv39 根页表（L2 页表）。
     * 每个 PTE 为 8 字节，所以一张表有 4096 / 8 = 512 个表项。
     * 先逐项清零，保证未显式设置的地址范围全部无效。 */
    la t0, boot_root_page
    li t1, 512
.Lzero_root:
    sd zero, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, .Lzero_root

    /* 在 L2 根页表中直接设置 R/W/X 的表项会成为叶子 PTE。
     * 一个 L2 表项控制 1 GiB：512 个 L1 表项 × 2 MiB。
     *
     * 0xcf = V | R | W | X | A | D：
     *   V：PTE 有效；R/W/X：可读、可写、可执行；
     *   A/D：已经访问、已经写入。
     * 这些 RWX 大页只供启动过渡使用，正式页表会收紧权限。 */
    la t0, boot_root_page
    li t1, 0xcf                 /* V|R|W|X|A|D */

    /* root[0]：VA 0x00000000..0x3fffffff
     *          -> PA 0x00000000..0x3fffffff。
     * 这一区间包含 QEMU test finisher、PLIC、UART 和 VirtIO MMIO。 */
    sd t1, 0(t0)

    /* PTE 的 PPN 从 bit 10 开始，而物理页号是 PA >> 12，所以：
     *   (PA >> 12) << 10 = PA >> 2。
     * 下列物理基址都按 1 GiB 对齐，满足 L2 大页对齐要求。 */

    /* root[1]：VA 0x40000000..0x7fffffff -> 相同 PA。 */
    li t2, 0x40000000
    srli t2, t2, 2
    or t2, t2, t1
    sd t2, 8(t0)

    /* root[2]：VA 0x80000000..0xbfffffff -> 相同 PA。
     * 当前低地址启动代码和 QEMU RAM 位于这个 1 GiB 区间内。 */
    li t2, 0x80000000
    srli t2, t2, 2
    or t2, t2, t1
    sd t2, 16(t0)

    /* root[3]：VA 0xc0000000..0xffffffff -> 相同 PA。 */
    li t2, 0xc0000000
    srli t2, t2, 2
    or t2, t2, t1
    sd t2, 24(t0)

    /* 再把相同的 4 GiB 物理空间映射到高半区 direct map：
     *   root[256] -> PA 0x00000000
     *   root[257] -> PA 0x40000000
     *   root[258] -> PA 0x80000000
     *   root[259] -> PA 0xc0000000
     * root[256] 对应的规范化 Sv39 虚拟地址是
     * 0xffffffc000000000，即 KERNEL_DIRECT_BASE。
     *
     * root[256] 的字节偏移为 256 * 8 = 2048，但 store 的 12 位
     * 有符号立即数最大只能表示 2047，所以先让 t0 前进 2040 字节，
     * 再用 8、16、24、32 作为剩余偏移。 */
    addi t0, t0, 2040
    sd t1, 8(t0)                /* root[256] = root[0] */
    ld t2, -2032(t0)            /* 读取 root[1] */
    sd t2, 16(t0)               /* root[257] = root[1] */
    ld t2, -2024(t0)            /* 读取 root[2] */
    sd t2, 24(t0)               /* root[258] = root[2] */
    ld t2, -2016(t0)            /* 读取 root[3] */
    sd t2, 32(t0)               /* root[259] = root[3] */

    /* 恢复 t0，使它再次指向根页表的物理起始地址。 */
    addi t0, t0, -2040

    /* 构造 satp：
     *   bits 63:60 MODE = 8，表示 Sv39；
     *   bits 59:44 ASID = 0；
     *   bits 43:0  PPN = 根页表物理地址 >> 12。
     * 写 satp 后地址翻译立即生效；低地址恒等映射保证当前指令流
     * 仍能继续执行，随后再显式跳到高半区。 */
    srli t0, t0, 12
    li t1, 8
    slli t1, t1, 60
    or t0, t0, t1
    csrw satp, t0
    /* 丢弃旧的地址翻译，并保证后续取指/访存使用刚写入的页表。 */
    sfence.vma zero, zero

    /* .Lhigh_entry 位于高半区，与当前低地址 PC 相距太远，普通直接跳转
     * 无法编码。链接器把完整的 64 位高地址写入下面的 .dword，这里先
     * 从仍由恒等映射访问的 .text.boot 中读出它，再间接跳转。 */
    la t0, .Lhigh_entry_address
    ld t0, 0(t0)
    jr t0

    .align 3
.Lhigh_entry_address:
    .dword .Lhigh_entry
    /* _start 的尺寸只计算低地址 .text.boot 部分；高地址入口属于 .text。 */
    .size _start, . - _start

    .section .text, "ax", @progbits
    .align 2
.Lhigh_entry:
    /* 现在 PC 已经是高半区虚拟地址。初始化 gp 时必须关闭 linker
     * relaxation，否则“给 gp 赋初值”的指令本身可能被优化成依赖 gp。 */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* 切换到链接器脚本预留的高半区启动栈。栈向低地址增长。 */
    la sp, __boot_stack_top

    /* .bss 是 ELF 的 NOBITS 区域，不在镜像中存放零字节，因此内核负责
     * 清零。此循环本身不使用栈，所以即使启动栈也位于 .bss 中仍安全。 */
    la t0, __bss_start
    la t1, __bss_end
.Lclear_bss:
    bgeu t0, t1, .Lbss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss
.Lbss_done:
    /* 按 C ABI 恢复 kernel_main 的两个参数。hart ID 可以直接恢复；
     * DTB 仍是物理地址，需要加 direct-map 基址，变成当前可解引用的
     * 高半区虚拟地址。临时页表已经映射了对应的前 4 GiB 物理空间。 */
    mv a0, s0
    li t0, 0xffffffc000000000
    add a1, s1, t0
    call kernel_main

    /* kernel_main 声明为 _Noreturn；若意外返回则进入 panic。 */
    call panic_returned_from_kernel_main
.Lhang:
    wfi
    j .Lhang

    /* 静态启动根页表：NOBITS 表示 ELF 文件不保存 4096 字节零数据，
     * 但加载段会为它保留内存。页表必须按 4 KiB（2^12）对齐。 */
    .section .bss.boot, "aw", @nobits
    .align 12
    .globl boot_root_page
boot_root_page:
    .space 4096

    .section .note.GNU-stack, "", @progbits
