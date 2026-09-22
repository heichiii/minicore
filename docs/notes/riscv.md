# What is RISCV?
*RISC-V is a family of CPU architecture standards. It defines instructions a processor can execute and, for systems that support them, rules for privilege modes,traps, and virtual memory.*
# Instructions and Registers
## Extensions
RISCV has base instructions and different extensions depending on the real requirements.
## Registers
- Base integer RV64I: 32 integer registers, x0–x31, each 64 bits wide.

- x0 is hardwired to zero, so there are 31 writable general-purpose integer registers plus the constant-zero register.

- If the F/D floating-point extensions are present: 32 floating-point registers, f0–f31.

- If the V vector extension is present: 32 vector registers, v0–v31.

- There is also a separate program counter (PC) and many CSRs control/status registers.

### ABI names

| Register | ABI name | Role                    |
| -------- | -------- | ----------------------- |
| x0       | zero     | constant 0              |
| x1       | ra       | return address          |
| x2       | sp       | stack pointer           |
| x3       | gp       | global pointer          |
| x4       | tp       | thread pointer          |
| x5       | t0       | temporary               |
| x6       | t1       | temporary               |
| x7       | t2       | temporary               |
| x8       | s0/fp    | saved / frame pointer   |
| x9       | s1       | saved                   |
| x10      | a0       | argument / return value |
| x11      | a1       | argument / return value |
| x12      | a2       | argument                |
| x13      | a3       | argument                |
| x14      | a4       | argument                |
| x15      | a5       | argument                |
| x16      | a6       | argument                |
| x17      | a7       | argument                |
| x18      | s2       | saved                   |
| x19      | s3       | saved                   |
| x20      | s4       | saved                   |
| x21      | s5       | saved                   |
| x22      | s6       | saved                   |
| x23      | s7       | saved                   |
| x24      | s8       | saved                   |
| x25      | s9       | saved                   |
| x26      | s10      | saved                   |
| x27      | s11      | saved                   |
| x28      | t3       | temporary               |
| x29      | t4       | temporary               |
| x30      | t5       | temporary               |
| x31      | t6       | temporary               |

# Privilege modes

## U S H M

The privilege levels are numbered from 0 (least privileged) to 3 (most privileged). The core levels are U, S, and M.

| Level | Name                   | Abbreviation | Typical Use Case                                             |
| ----- | ---------------------- | ------------ | ------------------------------------------------------------ |
| **0** | **User / Application** | **U-mode**   | Normal application code. Has the least privileges and is isolated from other applications and the OS. |
| **1** | **Supervisor**         | **S-mode**   | A conventional operating system kernel (e.g., Linux). It manages memory, I/O, and provides services to U-mode applications. |
| **2** | **Hypervisor**         | **H-mode**   | A hypervisor or virtual machine monitor (VMM). It manages multiple guest  operating systems. (Note: In the current spec, this is implemented as an extension to S-mode, called **HS-mode**). |
| **3** | **Machine**            | **M-mode**   | The highest privilege level. It has low-level access to the hardware and is the **only mandatory privilege level** for any RISC-V implementation. It is inherently trusted firmware. |

Code running in a lower-privileged mode cannot access the resources of a  higher-privileged mode directly. Attempting to do so (e.g., executing a  privileged instruction or accessing a protected CSR) raises an  exception, which is typically handled by a higher privilege level.

## Privilege level trsnsition

- **Traps (Entering a Higher Privilege Level):** A trap is an exception or interrupt that transfers control to a trap  handler running at a higher privilege level. For example, a U-mode  application making a system call (via the `ecall` instruction) traps into S-mode or M-mode. Traps can only move control to a **higher or equal** privilege level, never to a lower one on the initial entry.

- **Trap Returns (Returning to a Lower Privilege Level):** To return from a trap handler, special instructions are used:

  - **`mret`**: Return from M-mode. The hardware looks at the `MPP` (Machine Previous Privilege) field in the `mstatus` register to determine which privilege mode to return to.

  - **`sret`**: Return from S-mode. It uses the `SPP` (Supervisor Previous Privilege) field in the `sstatus` register.

- **Trap Delegation:** M-mode firmware can delegate certain traps to be handled directly by S-mode. This is done using the **`medeleg`** (Machine Exception Delegation) and **`mideleg`** (Machine Interrupt Delegation) CSRs. For instance, a page fault from a  U-mode application can be delegated to the S-mode OS kernel, avoiding an unnecessary trip to M-mode.

# Traps

*In RISC-V, a **trap** is any event that causes the CPU to abruptly stop its normal  instruction flow and transfer control to a handler running in a more  privileged mode.*

## Two main types

- **Exceptions (Synchronous)**: These occur **during the execution of an instruction** and are caused by the instruction itself. Examples include system calls (`ecall`), illegal instructions, division by zero, or memory access faults like  page faults. Because they are tied to a specific instruction, exceptions are predictable and synchronous with the program flow.
- **Interrupts (Asynchronous)**: These occur **outside the normal instruction flow** and can happen at any time. They are triggered by external hardware  events, such as a timer tick, a keyboard press, or a disk completing a  read/write request. Interrupts are asynchronous and are not directly  tied to the instruction being executed.

## ⚙️ The Core Trap Handling Mechanism

When a trap occurs, the hardware performs a series of "atomic" steps before  the software handler takes over. The exact operations depend on the  target privilege mode (Machine, Supervisor, or User), but the general  rules are as follows:

1. **Save the Program Counter**: The address of the instruction that was interrupted (for exceptions) or the next instruction to execute (for interrupts) is saved into the `xepc` register (e.g., `mepc` for Machine mode, `sepc` for Supervisor mode).
2. **Record the Cause**: The reason for the trap is encoded in the `xcause` register (e.g., `mcause`, `scause`). The highest bit (XLEN-1) indicates whether it was an interrupt (1) or  an exception (0), and the remaining bits hold the specific cause code.
3. **Store Trap Value (Optional)**: For certain exceptions like page faults or illegal instructions,  additional information (such as the faulting address or the instruction  itself) is written to the `xtval` register (e.g., `mtval`, `stval`).
4. **Update Status Registers**: The `xstatus` register (e.g., `mstatus`, `sstatus`) is updated. The previous privilege mode is saved in the `xPP` field (e.g., `MPP`, `SPP`). The previous global interrupt enable bit is saved in `xPIE`, and interrupts are globally disabled by clearing the `xIE` bit.
5. **Jump to the Handler**: The CPU jumps to the address specified in the trap vector register (`xtvec`). The `xtvec` register can be configured for **Direct** mode (all traps go to one address) or **Vectored** mode (each cause has its own entry point)

## 📋 Key Control and Status Registers (CSRs)

The trap mechanism relies on a set of CSRs that are accessible only in  privileged modes. The most important ones for Machine mode (M-mode) and  Supervisor mode (S-mode) are:

| CSR                       | Description                                                  |
| ------------------------- | ------------------------------------------------------------ |
| **`mtvec` / `stvec`**     | Trap Vector Base Address. Holds the address of the trap handler. |
| **`mepc` / `sepc`**       | Exception Program Counter. Saves the PC of the interrupted instruction. |
| **`mcause` / `scause`**   | Trap Cause. Encodes the reason for the trap (interrupt vs. exception and the specific cause code). |
| **`mtval` / `stval`**     | Trap Value. Holds additional information like the faulting address or instruction. |
| **`mstatus` / `sstatus`** | Status Register. Holds global interrupt enable (`xIE`), previous interrupt enable (`xPIE`), and previous privilege mode (`xPP`). |
| **`mideleg` / `medeleg`** | Interrupt/Exception Delegation. Controls which traps are delegated to a lower privilege mode (e.g., from M-mode to S-mode). |

## ⚖️ Trap Causes, Priority, and Delegation

- **Cause Codes**: The `xcause` register uses specific codes to identify the trap. For example, cause code `8` in `mcause` indicates an environment call from U-mode, while cause code `2` indicates an illegal instruction.

**Priority Rules**: When multiple exceptions or interrupts are pending simultaneously, RISC-V defines a strict priority order. **All interrupts have higher priority than any synchronous exception**. Among interrupts, the standard priority order (from highest to lowest)  is: Machine External, Machine Software, Machine Timer, Supervisor  External, Supervisor Software, and Supervisor Timer. For exceptions, the priority is defined in the privileged spec, with instruction address  misaligned being the highest priority and environment calls being the  lowest.

**Delegation**: By default, all traps are handled in Machine mode. However, M-mode  software can delegate certain traps to Supervisor mode by setting the  corresponding bits in the `medeleg` (for exceptions) and `mideleg` (for interrupts) registers. This allows an OS kernel running in S-mode to handle its own page  faults and timer interrupts without involving the M-mode firmware.

## 🔄 Returning from a Trap

After the trap handler finishes its work, it must return control to the  interrupted program. This is done using specific return instructions:

- **`mret`**: Used to return from a trap taken into M-mode.
- **`sret`**: Used to return from a trap taken into S-mode.

When executing an `xret` instruction, the hardware reverses the steps taken during trap entry: it restores the privilege mode from `xPP`, restores the interrupt enable state from `xPIE`, and sets the PC to the value saved in `xepc`.

## 📝 Specific Rules for Trap Values (`xtval`)

The `xtval` register is not always written; its content is strictly defined for specific exceptions:

- For **load, store, and instruction page-faults, access-faults, and misaligned exceptions**, `xtval` must be written with the faulting virtual address.
- For **illegal-instruction exceptions**, `xtval` must be written with the faulting instruction.
- For **breakpoint exceptions** (other than those caused by the `ebreak` instruction), `xtval` is written with the address of the breakpoint.
- For all other exceptions, `xtval` is typically set to zero.



# Memory