# Platform-level interrupt controller
总结一下：

* **PLIC 是外设中断控制器**，QEMU `riscv64 virt` 中 UART、VirtIO、PCIe 等外设 IRQ 都通常先进入 PLIC，再送到 CPU。
* **不配置 PLIC**：这些外设即使产生 IRQ，CPU 通常也收不到；但设备本身仍可通过轮询方式使用。
* **不能只配 `mie/mstatus` 就绕过 PLIC**。`mie.MEIE`、`mstatus.MIE` 只是让 CPU 允许接收外部中断，并不能决定哪个设备 IRQ 能送进来。
* **Timer / Software interrupt 不经过 PLIC**，因此这两类中断可以独立工作。
* 使用 PLIC 的最小流程是：

```text
设备打开中断
   ↓
PLIC 设置 priority
   ↓
PLIC enable 对应 IRQ
   ↓
PLIC threshold 设置合适
   ↓
CPU 打开 MEIE/MIE
   ↓
设备产生 IRQ
   ↓
CPU 收到 MEIP
   ↓
读取 PLIC claim 得到 IRQ 号
   ↓
处理中断
   ↓
写回 complete
```

一句话：

> **PLIC 决定“哪个外设中断送给 CPU”，`mie/mstatus` 决定“CPU 是否接受这个外部中断”。**
