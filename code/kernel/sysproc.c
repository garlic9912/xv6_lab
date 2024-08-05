#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sysinfo.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}



// 跟踪系统调用，返回pid和函数返回值
// $ trace 32 grep hello README
// 3: syscall read -> 1023
uint64
sys_trace(void)
{
  int n;
  // 获取系统调用的参数
  if(argint(0, &n) < 0)
    return -1;
  myproc()->mask = n;
  return 0;
}



uint64
sys_sysinfo(void) {
  struct proc *p = myproc();
  struct sysinfo info;
  uint64 addr;
  
  // 获取数据
  info.freemem = get_freemem();
  info.nproc = get_proc();

  // 系统调用第n=0个参数的用户空间地址
  if (argaddr(0, &addr) < 0) return -1;

  // p->pagetable：当前进程的页表
  // addr：用户空间目的地址
  // (char *)&info：内核空间的源地址
  // sizeof(info)：复制的长度
  if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
