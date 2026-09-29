Conclusion: MiniCore runs in S-mode, but it can access UART0 directly through
  MMIO because OpenSBI’s PMP configuration permits S-mode access to 0x10000000.

  Therefore:

  - No SBI call is needed for UART output.
  - UART0[0] = ch directly writes the UART transmit register.
  - With paging currently disabled, the physical address is used directly.
  - After enabling paging, the kernel must map the UART MMIO range.
  - SBI is only necessary for hardware facilities reserved to M-mode, such as
    the machine timer—not for every hardware access.
