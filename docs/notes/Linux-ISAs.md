In conclusion, Linux does **not** have a single formal HAL with a fixed ABI. Instead, it uses a **distributed architecture abstraction layer**:

- ISA-specific code lives in `arch/<arch>/`.
- Shared defaults live in `include/asm-generic/`.
- Generic kernel code calls a common set of abstractions like `local_irq_save()`, `readl()`, `smp_processor_id()`, etc.
- Those abstractions are implemented using macros, `static inline` functions, extern functions, weak symbols, and subsystem ops tables such as `struct irq_chip` or `struct dma_map_ops`.
- There is **no runtime binary ABI translation**; the kernel is compiled separately for each ISA, so the contract is **source-level**, not binary-level.
- The interface is semantically consistent, but not textually identical across architectures. Optional features are handled with `Kconfig` and `#ifdef`.

So Linux supports many ISAs by keeping the core kernel and most drivers architecture-independent, while pushing hardware-specific details into `arch/` behind a common compile-time contract. That is the essence of its “HAL”: a distributed porting layer, not a monolithic one.