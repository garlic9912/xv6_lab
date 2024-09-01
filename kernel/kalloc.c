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


char* a[] = {"kmemCPU0", "kmemCPU1", "kmemCPU2",
            "kmemCPU3", "kmemCPU4", "kmemCPU5",
            "kmemCPU6", "kmemCPU7"};

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];


void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, a[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

// 获取cpu的id号
int getCpuId() {
  return cpuid();
}


void
kfree(void *pa)
{
  struct run *r;
  int cpu_id = getCpuId();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}


// 扩容空闲页表
struct run*
kmemalloc(void) {
  struct run *r;

  for (int i = 0; i < NCPU; i++) {
    acquire(&kmem[i].lock);
    // 空闲列表是否有余
    r = kmem[i].freelist;
    if(r)
      kmem[i].freelist = r->next;
    release(&kmem[i].lock);

    if (!r) {
      continue;
    } else {
      memset((char*)r, 5, PGSIZE); // fill with junk
      break;
    }   
  }
  return r;
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id = getCpuId();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  release(&kmem[cpu_id].lock);

  // 不存在空闲页，则窃取空闲页
  if (!r) {
    r = kmemalloc();
  } 

  // 存在空闲页
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}


