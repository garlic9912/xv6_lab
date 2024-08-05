// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// 定义了一个链表节点，用于表示一个空闲的内存页
struct run {
  struct run *next;
};


// 包含一个自旋锁 lock，用于在多处理器环境下保护空闲内存页链表的访问
// freelist 指向空闲内存页链表的头节点
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


// 初始化内存分配器，调用 initlock() 初始化 kmem 结构中的锁
// 调用 freerange() 将从 end 到 PHYSTOP 之间的物理内存页加入到空闲页链表中
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}


// 该函数遍历从 pa_start 到 pa_end 之间的物理内存地址，将每一个完整的页调用 kfree() 进行释放，从而将它们加入到空闲页链表中
// 使用 PGROUNDUP() 确保 pa_start 地址对齐到页边界
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}


// 释放一个物理内存页，将其加入到空闲页链表中
void
kfree(void *pa)
{
  struct run *r;

  // 首先对传入的地址 pa 进行验证，确保它是对齐的，并且在合法的内存范围内
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // 使用 memset 填充内存页以检测悬空引用，然后将页加入到空闲页链表中
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}



// 从空闲页链表中分配一个物理内存页
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;

  // 如果有空闲页可用，则从链表中取出第一个页，并将该页从链表中移除
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  // 如果成功分配了页，使用 memset 填充它以检测错误
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


// 获取空闲内存量
uint64
get_freemem(void) {
  struct run *r = kmem.freelist;
  uint64 n = 0;

  acquire(&kmem.lock);
  while (r) {
    n++;
    r = r->next;
  }
  release(&kmem.lock);

  return n * PGSIZE;
}
