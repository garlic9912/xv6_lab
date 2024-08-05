/*
这段代码定义了一个物理内存布局和一些重要的内存地址，用于一个基于 RISC-V 架构的操作系统

内存布局概述
    00001000: 启动ROM，QEMU提供。
    02000000 (CLINT): Core Local Interruptor (CLINT)，用于管理定时器和软件中断。
    0C000000 (PLIC): Platform-Level Interrupt Controller (PLIC)，管理外部中断。
    10000000 (uart0): UART0 的寄存器基地址，用于串口通信。
    10001000 (virtio disk): VirtIO磁盘设备的内存映射基地址。
    80000000: 内核加载地址。启动ROM在机器模式下跳转到此处执行。

内核物理内存使用
    80000000: 内核代码（text 和 data 段）的起始地址。
    end: 内核代码段结束后的内存区域，用于内核的内存分配。
    PHYSTOP: 内核可以使用的物理内存的结束地址。

特定设备的内存地址定义
    UART0: 0x10000000 是 UART0 的基地址，用于内核与外界的串行通信。
    VIRTIO0: 0x10001000 是 VirtIO 磁盘设备的基地址。
    CLINT: 定义了 CLINT 的基地址以及与定时器相关的寄存器地址。
    CLINT_MTIMECMP(hartid): 每个硬件线程（hart）的 mtimecmp 寄存器地址。
    CLINT_MTIME: 全局定时器寄存器。
    PLIC: 0x0c000000 是 PLIC 的基地址，用于管理设备中断。
    PLIC_PRIORITY, PLIC_PENDING, PLIC_MENABLE(hart) 等定义了用于管理中断优先级、挂起状态、使能状态等的寄存器地址。

内核地址空间布局
    KERNBASE: 内核的基地址，0x80000000。
    PHYSTOP: 内核和用户可以使用的物理内存的结束地址，定义为 KERNBASE + 128MB。
    TRAMPOLINE: 定义了内核和用户空间中最高的地址，通常用于存放一个特殊的跳板代码页（trampoline page）。
    KSTACK(p): 定义了每个进程内核栈的起始地址。
    用户空间布局
    TRAPFRAME: 定义了内核和用户共享的内存地址，用于存储进程的陷阱帧（trapframe）。
    TRAMPOLINE: 同样的跳板代码页地址，在用户空间中映射到相同的物理地址。
这些定义帮助内核管理和控制系统中的物理和虚拟内存，处理各种硬件设备，并为用户进程提供适当的内存布局。
*/



// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// local interrupt controller, which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts programmable interrupt controller here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
