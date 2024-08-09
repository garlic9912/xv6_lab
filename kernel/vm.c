// 代码实现了一个内核页表管理系统，主要功能包括页表的初始化、映射、地址转换、页表清理等操作

#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "proc.h"



/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S





// == 内核页表初始化 ==
void
kvminit()
{
  // 内核页表通过 kalloc() 分配
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  /*
  使用 kvmmap() 函数将一系列重要的物理地址
  （如 UART0, VirtIO, CLINT, PLIC）
  映射到内核页表中
  */

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // 最后，将跳板代码（trampoline）映射到内核的最高虚拟地址
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}








// == 启用内核页表 ==
// 设置硬件的页表寄存器指向内核的页表
// 通过 sfence_vma() 刷新页表缓存，启用分页机制
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}









// == 页表项的查找和创建 ==
// 根据虚拟地址 va 在 pagetable 中查找对应的页表项（PTE）
// RISC-V 的 Sv39 采用三级页表，每级页表包含 512 个 64 位的 PTE
// 若 alloc 参数为真且所需的页表页不存在，则创建它们
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");
  // 循环遍历三级页表
  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}








// == 虚拟地址到物理地址的转换 ==
// 查找并返回虚拟地址 va 对应的物理地址（仅适用于用户页）
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}









// == 内核页表的地址映射 ==
// 在内核页表中添加一段虚拟地址到物理地址的映射。此函数主要在系统引导时使用
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}








// == 虚拟地址到物理地址的直接转换 ==
// 该函数将内核虚拟地址转换为物理地址，假设虚拟地址是页对齐的
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  pte = walk(myproc()->kernel_pt, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}







// == 建页表项映射 ==
// 在给定的页表中创建从虚拟地址到物理地址的映射
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}






// == 取消页表项映射 ==
// 删除 pagetable 中从 va 开始的 npages 页的映射，并且可以选择性地释放相应的物理内存
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}





// == 页表的创建 ==
// 创建一个空的页表
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}







// == 加载用户初始代码 ==
// 将用户初始代码加载到用户页表的地址 0 处
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}







// == 增加进程的地址空间 ==
// 为进程分配物理内存并更新页表，支持从 oldsz 增加到 newsz
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}







// == 减少进程的地址空间 ==
// 减少进程的地址空间并释放不再使用的物理内存
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}




// == 递归释放页表 ==
// 递归地释放页表项，清理内存
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];

    // 检查 PTE 是否有效（即 PTE_V 位被设置）
    // 如果 PTE 有效，但它既不是可读、可写，也不是可执行的（即非叶子节点），那么它指向下一级页表。
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}






// == 释放用户页表 ==
// 释放用户内存页，然后释放页表
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}







// == 复制用户页表和内存 ==
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}



// == 使 PTE 对用户不可访问 ==
// 将指定的 PTE 标记为对用户不可访问
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}




// == 内存拷贝函数 ==
// 这些函数实现了内核与用户空间之间的数据拷贝功能
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}




int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
}


int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
}







void _vmprint(pagetable_t pt, int deep) {
  for (int i = 0; i < 512; i++) {
    // 页表中的每个pte
    pte_t pte = pt[i];
    // 有效的pte
    if (pte && PTE_V) {
      // 打印深度
      for (int j = 1; j <= deep; j++) {
        printf("..");
        if (j != deep) printf(" ");
      }
      // 非叶子结点(前两级页表)
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // 打印信息
        pagetable_t pa = (pagetable_t)PTE2PA(pte);
        printf("%d: pte %p pa %p\n", i, pte, pa);
        // 递归
        _vmprint(pa, deep+1);
        // 叶子结点(最后的页表)  
      } else {
        // 打印信息
        pagetable_t pa = (pagetable_t)PTE2PA(pte);
        printf("%d: pte %p pa %p\n", i, pte, pa);
      }
    }
  }
}

void 
vmprint(pagetable_t pt) {
  // 打印参数
  printf("page table %p\n", pt);
  _vmprint(pt, 1);
}



// == 进程内核页表用于映射的辅助函数 ==
void
proc_kvmmap(pagetable_t pt, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pt, va, sz, pa, perm) != 0)
    panic("proc_kvmmap");
}


// == 进程的内核页表初始化 ==
pagetable_t
proc_kernel_pt(void){
  pagetable_t pagetable;
  // 创建空页表
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // 映射
  proc_kvmmap(pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  proc_kvmmap(pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  proc_kvmmap(pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  proc_kvmmap(pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  proc_kvmmap(pagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  proc_kvmmap(pagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  proc_kvmmap(pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);  

  return pagetable;
}



// == 将进程的内核页表加载进satp ==
void
proc_kvminithart(pagetable_t pt)
{
  w_satp(MAKE_SATP(pt));
  sfence_vma();
}



// == 释放进程的内核页表 ==
void
proc_freewalk(pagetable_t pagetable)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      pagetable[i] = 0;
      if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        uint64 child = PTE2PA(pte);
        proc_freewalk((pagetable_t)child);
      }
    }
  }
  kfree((void*)pagetable);
}










void
u2kvmcopy(pagetable_t pagetable, pagetable_t kernel_pt, uint64 start, uint64 end){
  pte_t *pte_from, *pte_to;

  for (uint64 i = PGROUNDUP(start); i < end; i += PGSIZE){
    // 用户页表此pte是否存在且有效
    if((pte_from = walk(pagetable, i, 0)) == 0)
      panic("u2kvmcopy: src pte does not exist");
    // 判断内核是否存在此映射，无则创建
    if((pte_to = walk(kernel_pt, i, 1)) == 0)
      panic("u2kvmcopy: pte walk failed");
    
    uint64 pa = PTE2PA(*pte_from);
    uint flags = (PTE_FLAGS(*pte_from)) & (~PTE_U);
    // 添加一个pte
    *pte_to = PA2PTE(pa) | flags;
  }
}


