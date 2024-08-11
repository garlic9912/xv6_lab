// 操作系统中断和异常处理机制

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// 锁，保护时钟中断的全局变量 ticks
struct spinlock tickslock;
uint ticks;

// 1.trampoline[] 用于在内核和用户空间之间切换时保存特定的代码段
// 2.uservec[] 是处理用户模式陷阱（trap）的入口地址，定义在汇编文件trampoline.S中
// 当用户模式的程序发生系统调用或者异常时，处理器会跳转到 uservec 处开始执行相应的陷阱处理逻辑
// 3.userret[] 是从内核返回到用户模式时的一个入口地址
// 当内核完成了系统调用或异常的处理工作准备返回到用户空间时，处理器会跳转到 userret 处
extern char trampoline[], uservec[], userret[];

// kernelvec 是一个处理器在内核模式下发生陷阱时的入口函数地址
// 外部函数，实现是在其他文件（通常是汇编文件
void kernelvec();

extern int devintr();

void trapinit(void)
{
  initlock(&tickslock, "time");
}

// // 设置 stvec 寄存器为 kernelvec 的地址，这告诉处理器在内核态发生异常时应该跳转到 kernelvec
void trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

// 处理从用户模式发生的中断、异常或系统调用
void usertrap(void)
{
  int which_dev = 0;

  // 检查当前异常或中断的来源是否为用户模式
  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 将 stvec 设置为 kernelvec，将后续的中断和异常转发到内核处理
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // 保存当前用户程序计数器 epc
  p->trapframe->epc = r_sepc();

  // 如果 scause 是 8，表示发生了系统调用
  // 处理完系统调用后需要将 epc 向前移动 4 个字节，以跳过 ecall 指令
  if (r_scause() == 8)
  {
    // system call

    if (p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();

    // 如果是中断，通过 devintr 函数处理
  }
  else if ((which_dev = devintr()) != 0)
  {
    // ok
  }
  else
  {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if (p->killed)
    exit(-1);

  // 中断源是计时器中断
  if (which_dev == 2 && p->nticks != 0 && p->flag == 0)
  {
    p->time++;
    // 判断是否开启了alarm
    if (p->time == p->nticks)
    {
      memmove(&p->alarmframe, p->trapframe, sizeof(struct trapframe));
      p->trapframe->epc = p->addr;
      p->flag = 1;
    }
    yield();
  }
  // 返回用户态
  usertrapret();
}

// 从内核态返回用户态的函数
void usertrapret(void)
{
  struct proc *p = myproc();

  // 在从内核态返回到用户态的过程中，操作系统会进行一系列重要的操作
  // 例如切换页表、恢复用户态寄存器、设置程序计数器等
  // 这些操作必须连续且不可中断
  // 如果在此过程中发生中断，可能会导致上下文切换的不完整或不一致，从而引发严重的系统问题
  intr_off();

  // stvec寄存器保存了处理器在发生陷阱（trap）时跳转的地址
  // 设置 stvec 为用户模式的中断处理程序 uservec
  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // sepc 是 RISC-V 的一个寄存器，用来保存产生异常或陷入（trap）时的程序计数器（PC）的值
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // 通过跳转到 trampoline.S 中的代码，完成从内核态返回到用户态的过程
  // jump to trampoline.S at the top of memory, which
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

//  处理内核模式下发生的中断或异常
void kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  // 检查当前的模式是否为内核模式
  if ((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  // 确保中断在进入该函数时已经被禁用
  if (intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  // 通过 devintr 处理中断，如果未识别到中断源，则触发 panic
  if ((which_dev = devintr()) == 0)
  {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // 如果是计时器中断并且当前进程处于运行状态，调用 yield 让出 CPU
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // 恢复 sepc 和 sstatus，准备返回内核的正常执行流程
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// clockintr 是处理时钟中断的函数
// 每次中断发生时，增加 ticks 计数器并唤醒可能在等待 ticks 变化的进程
void clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// devintr 函数检查中断的来源并处理它们
// 对于外部中断（如来自 UART 或 VirtIO 磁盘），调用相应的中断处理程序
// 对于计时器中断，通过清除 sip 寄存器中的 SSIP 位来确认中断。
int devintr()
{
  uint64 scause = r_scause();

  if ((scause & 0x8000000000000000L) &&
      (scause & 0xff) == 9)
  {
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if (irq == UART0_IRQ)
    {
      uartintr();
    }
    else if (irq == VIRTIO0_IRQ)
    {
      virtio_disk_intr();
    }
    else if (irq)
    {
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
      plic_complete(irq);

    return 1;
  }
  else if (scause == 0x8000000000000001L)
  {
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if (cpuid() == 0)
    {
      clockintr();
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  }
  else
  {
    return 0;
  }
}
