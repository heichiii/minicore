The Device Tree ecosystem is a family of related concepts used mainly in Linux, U-Boot, BSD, and embedded systems to describe hardware separately from kernel source code.

A useful way to understand it is:

```text
DTS / DTSI
   │
   │  compiled by dtc
   ▼
DTB
   │
   │  loaded by bootloader
   ▼
FDT in memory
   │
   │  parsed by kernel
   ▼
Linux device model / drivers
```

### Core concepts

| Term              | Full name                  | Meaning                                                     |
| ----------------- | -------------------------- | ----------------------------------------------------------- |
| **Device Tree**   | Device Tree                | A hierarchical description of hardware                      |
| **DTS**           | Device Tree Source         | Human-readable source form                                  |
| **DTSI**          | Device Tree Source Include | Reusable/include fragment of DTS                            |
| **DTB**           | Device Tree Blob           | Compiled binary Device Tree                                 |
| **FDT**           | Flattened Device Tree      | Binary data format used by DTB                              |
| **DTC**           | Device Tree Compiler       | Tool converting DTS ↔ DTB                                   |
| **DTBO**          | Device Tree Blob Overlay   | Binary overlay applied to a base DTB                        |
| **DTO / Overlay** | Device Tree Overlay        | Mechanism for modifying a base tree                         |
| **OF**            | Open Firmware              | Historical specification/API behind Device Tree conventions |

The most important distinction is:

> **DTS is source code, DTB is the compiled file, and FDT is the binary format/layout used by the DTB.**

---

## 1. Device Tree

A **Device Tree** describes the hardware topology of a system.

For example:

```dts
/ {
    model = "Example Board";

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x40000000>;
    };

    uart@10000000 {
        compatible = "ns16550a";
        reg = <0x10000000 0x100>;
        status = "okay";
    };
};
```

It describes things such as:

* CPUs
* RAM
* buses
* UART
* I²C
* SPI
* GPIO
* clocks
* interrupts
* Ethernet
* displays
* regulators
* sensors

It describes **hardware**, not driver implementation.

The kernel uses properties such as:

```text
compatible
reg
interrupts
clocks
status
```

to discover devices and bind them to appropriate drivers.

---

## 2. DTS — Device Tree Source

A `.dts` file is the main human-readable Device Tree file for a board.

Example:

```text
my-board.dts
```

Typical structure:

```dts
/dts-v1/;

/ {
    compatible = "vendor,my-board";

    chosen {
        bootargs = "console=ttyS0,115200";
    };

    uart0: serial@1000 {
        compatible = "vendor,uart";
        reg = <0x1000 0x100>;
    };
};
```

You can think of DTS as roughly analogous to source code:

```text
C source        → object/binary
DTS source      → DTB binary
```

---

## 3. DTSI — Device Tree Source Include

A `.dtsi` file is normally a reusable Device Tree fragment.

For example:

```text
soc.dtsi
board-common.dtsi
chip-family.dtsi
```

A SoC vendor might define common hardware in:

```dts
soc.dtsi
```

such as:

```dts
/ {
    soc {
        uart0: serial@1000 {
            compatible = "vendor,uart";
            reg = <0x1000 0x100>;
            status = "disabled";
        };
    };
};
```

A board DTS can include it:

```dts
#include "soc.dtsi"

&uart0 {
    status = "okay";
};
```

This allows one SoC description to be reused by many boards.

A common hierarchy looks like:

```text
SoC family
   │
   └── soc.dtsi
        │
        ├── board-a.dts
        ├── board-b.dts
        └── board-c.dts
```

The `.dtsi` extension is a convention rather than a fundamentally different syntax.

---

## 4. DTC — Device Tree Compiler

`dtc` is the Device Tree Compiler.

It compiles:

```text
DTS → DTB
```

For example:

```bash
dtc -I dts -O dtb board.dts -o board.dtb
```

It can also decompile:

```text
DTB → DTS
```

using:

```bash
dtc -I dtb -O dts board.dtb -o decoded.dts
```

So:

```text
              dtc
board.dts ───────────► board.dtb
              ▲
              │
              └── decompile
```

---

## 5. DTB — Device Tree Blob

A `.dtb` file is the compiled binary representation.

For example:

```text
rk3568-board.dtb
imx8mp-board.dtb
bcm2711-rpi-4-b.dtb
```

It is normally loaded by the bootloader together with the kernel:

```text
Bootloader
   │
   ├── Linux kernel
   ├── initramfs
   └── DTB
```

Conceptually the bootloader invokes the kernel with something like:

```text
kernel_address
dtb_address
initrd_address
```

The kernel then parses the DTB to discover the hardware.

---

# 6. FDT — Flattened Device Tree

This term often causes confusion.

**FDT is primarily the binary representation/format of a Device Tree.**

The FDT format contains:

```text
+----------------------+
| FDT Header           |
+----------------------+
| Memory reservations  |
+----------------------+
| Structure block      |
+----------------------+
| Strings block        |
+----------------------+
```

A `.dtb` file is therefore usually:

> a file containing an FDT-formatted Device Tree.

So people often use **DTB** and **FDT** almost interchangeably, but technically:

```text
DTB = blob/file
FDT = binary representation/format
```

For example:

```text
board.dtb
```

is a file whose contents use the:

```text
Flattened Device Tree format
```

---

# 7. Why "Flattened"?

The logical Device Tree is hierarchical:

```text
/
├── cpus
│   ├── cpu@0
│   └── cpu@1
│
├── memory@80000000
│
└── soc
    ├── uart@1000
    ├── gpio@2000
    └── i2c@3000
```

But memory doesn't contain C-style nested objects or pointers.

Instead, the hierarchy is serialized into a flat sequence:

```text
FDT_BEGIN_NODE
  "/"

  FDT_BEGIN_NODE
    "cpus"

    FDT_BEGIN_NODE
      "cpu@0"
    FDT_END_NODE

  FDT_END_NODE

FDT_END_NODE
```

Hence:

**Flattened Device Tree**.

---

# 8. Device Tree Overlay

Sometimes you don't want to replace the whole Device Tree.

Instead, you want to modify part of it.

For example, your base board has:

```dts
&i2c1 {
    status = "disabled";
};
```

An expansion board may enable it:

```dts
&i2c1 {
    status = "okay";

    sensor@48 {
        compatible = "vendor,temp-sensor";
        reg = <0x48>;
    };
};
```

This modification can be represented as a **Device Tree Overlay**.

Conceptually:

```text
Base DTB
   +
Overlay
   =
Modified Device Tree
```

This is especially common with:

* Raspberry Pi
* BeagleBone
* FPGA systems
* expansion boards
* dynamically configurable embedded hardware

---

# 9. DTBO

A compiled Device Tree Overlay often uses:

```text
.dtbo
```

So:

```text
overlay.dts
     │
     │ dtc
     ▼
overlay.dtbo
```

The bootloader or kernel can apply the `.dtbo` to a base `.dtb`.

For example:

```text
board.dtb
   +
camera.dtbo
   =
Device Tree with camera enabled
```

---

# 10. Nodes

A Device Tree is composed of **nodes**.

For example:

```dts
uart@1000 {
    ...
};
```

Here:

```text
uart@1000
```

is a node.

Typically:

```text
node-name@unit-address
```

Examples:

```text
serial@1000
gpio@2000
i2c@3000
ethernet@ff540000
```

The part after `@` normally corresponds to the device address.

---

# 11. Properties

Nodes contain properties.

Example:

```dts
uart@1000 {
    compatible = "ns16550a";
    reg = <0x1000 0x100>;
    interrupts = <5>;
    status = "okay";
};
```

Here:

```text
compatible
reg
interrupts
status
```

are properties.

Values can be:

```text
strings
integer cells
byte arrays
string lists
references
```

Examples:

```dts
status = "okay";

clock-frequency = <24000000>;

compatible = "vendor,device", "generic-device";

mac-address = [00 11 22 33 44 55];
```

---

# 12. `compatible`

`compatible` is one of the most important properties.

Example:

```dts
compatible = "vendor,my-uart";
```

Linux drivers contain matching tables such as:

```c
static const struct of_device_id uart_of_match[] = {
    { .compatible = "vendor,my-uart" },
    {}
};
```

The kernel effectively does:

```text
Device Tree compatible
          │
          ▼
"vendor,my-uart"
          │
          ▼
driver's of_match_table
          │
          ▼
Driver probe()
```

So `compatible` connects the hardware description to a driver.

---

# 13. `reg`

`reg` describes addresses and sizes.

For example:

```dts
uart@1000 {
    reg = <0x1000 0x100>;
};
```

Conceptually:

```text
base address = 0x1000
size         = 0x100
```

But the exact interpretation depends on the parent's:

```text
#address-cells
#size-cells
```

For example:

```dts
soc {
    #address-cells = <1>;
    #size-cells = <1>;

    uart@1000 {
        reg = <0x1000 0x100>;
    };
};
```

---

# 14. Phandle

Nodes sometimes need to refer to one another.

For example:

```dts
clk0: clock@100 {
    ...
};

uart@1000 {
    clocks = <&clk0>;
};
```

`&clk0` gets converted into an integer identifier known as a:

**phandle**.

Conceptually:

```text
uart
 │
 └── clocks
       │
       ▼
     clk0
```

This is extensively used for:

* clocks
* GPIOs
* regulators
* interrupts
* DMA
* resets
* IOMMUs

---

# 15. Labels

In:

```dts
uart0: serial@1000 {
};
```

`uart0` is a **label**.

The node name is:

```text
serial@1000
```

The label:

```text
uart0
```

allows another part of the source to write:

```dts
&uart0 {
    status = "okay";
};
```

Labels are mainly a source-level convenience.

---

# 16. Bindings

A Device Tree Binding specifies how a particular hardware device should be described.

For example, a UART binding might define:

```text
required:
    compatible
    reg
    interrupts
    clocks
```

Today Linux bindings are commonly expressed using YAML schemas.

For example conceptually:

```yaml
properties:
  compatible:
    const: vendor,my-uart

  reg:
    maxItems: 1

required:
  - compatible
  - reg
```

The relationship is:

```text
Hardware device
      │
      ▼
Device Tree Binding
      │ defines
      ▼
valid DTS properties
      │
      ▼
Device Tree node
```

Bindings are therefore similar to a **schema or interface contract** for Device Tree descriptions.

---

# 17. `dt-schema`

Modern Linux Device Tree development also uses **dt-schema**.

It validates Device Tree descriptions against YAML bindings.

Common kernel commands include:

```bash
make dt_binding_check
```

and:

```bash
make dtbs_check
```

Conceptually:

```text
Binding YAML
     │
     ▼
   Schema
     │
     ▼
DTS / DTB validation
```

---

# 18. Open Firmware / OF

You will often encounter Linux APIs such as:

```c
of_find_node_by_name()
of_property_read_u32()
of_device_is_compatible()
struct device_node
```

The `of_` prefix stands for:

**Open Firmware**.

Device Tree concepts originated from IEEE Open Firmware conventions.

That's why Linux terminology includes:

```text
OF node
OF property
OF match table
```

even when Open Firmware itself is not involved in the boot process.

In Linux, you may therefore see:

```text
OF ≈ Device Tree-related APIs
```

in many contexts.

---

# 19. libfdt

`libfdt` is a C library for manipulating FDT blobs.

It is commonly used by:

* U-Boot
* bootloaders
* firmware
* virtualization software

Typical functions include:

```c
fdt_check_header()
fdt_path_offset()
fdt_getprop()
fdt_setprop()
fdt_add_subnode()
```

So:

```text
        FDT blob
           │
           ▼
        libfdt
           │
     ┌─────┴─────┐
     ▼           ▼
   read        modify
```

`libfdt` is part of the broader Device Tree Compiler project.

---

# 20. Linux representation after parsing

Once Linux receives the FDT, it parses it.

Conceptually:

```text
DTB / FDT
   │
   ▼
struct device_node
   │
   ▼
Linux device objects
   │
   ▼
drivers
```

So there are several representations:

```text
DTS text
   │
   ▼
FDT binary / DTB
   │
   ▼
Linux internal Device Tree representation
   │
   ▼
struct device / platform_device
```

These should not be confused with one another.

---

# 21. Boot process

A typical ARM/Linux boot flow looks like:

```text
Power on
   │
   ▼
Boot ROM
   │
   ▼
Bootloader
e.g. U-Boot
   │
   ├── loads Image
   ├── loads board.dtb
   ├── possibly modifies DTB
   └── possibly applies DTBO
   │
   ▼
Linux kernel
   │
   ├── parses FDT
   ├── discovers CPU / RAM
   ├── discovers devices
   └── binds drivers
   │
   ▼
Userspace
```

U-Boot may modify the Device Tree before Linux starts.

For example, it can add:

```dts
/chosen {
    bootargs = "...";
};
```

or memory information.

---

# 22. Common file relationships

A realistic Linux kernel source tree might contain:

```text
arch/arm64/boot/dts/vendor/
│
├── soc1234.dtsi
├── soc1234-board-a.dts
├── soc1234-board-b.dts
└── soc1234-board-c.dts
```

Build process:

```text
soc1234.dtsi
      │
      ├─────────────┐
      ▼             ▼
board-a.dts     board-b.dts
      │             │
      ▼             ▼
board-a.dtb     board-b.dtb
```

---

# 23. The terminology in one picture

The whole family can be summarized like this:

```text
                  Device Tree
                       │
       ┌───────────────┴────────────────┐
       │                                │
  Source representation            Binary representation
       │                                │
       ▼                                ▼
      DTS                              FDT
       │                                │
       ├── includes DTSI                └── commonly stored as
       │                                      DTB
       │
       │       dtc
       └──────────────────────────────►
       
Overlay source                     Overlay binary
     DTS                                DTBO
       │                                 │
       └────────── dtc ─────────────────►


Supporting concepts:

DTC       = compiler
libfdt    = binary manipulation library
Bindings  = specification/schema for nodes
dt-schema = binding validation framework
OF        = historical/Open Firmware terminology
phandle   = cross-node reference identifier
```

The most useful mental model is:

```text
DTS  = source
DTSI = include/header-like source
DTC  = compiler
DTB  = compiled file
FDT  = binary format/data structure
DTBO = compiled patch/overlay
Binding = schema/specification
libfdt = API for manipulating the binary tree
OF = Linux's historical Device Tree API terminology
```

One subtle point matters a lot: **Device Tree is not a list of Linux devices.** It is primarily a description of hardware topology and relationships. Linux interprets that description and then creates its own internal device structures and binds drivers to them.
