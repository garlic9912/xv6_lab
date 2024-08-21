# 1.Copy-on-Write Fork for xv6（ 难度：hard）

- **解题步骤**

跟着提示一步一步来

- 在***kernel/riscv.h***中选取`PTE`中的保留位定义标记一个页面是否为COW页面的标志位

```c
// copy-on-write标记位
#define COW (1L << 8)
```

- 在***kalloc.c***中进行如下修改

定义引用计数的全局变量`paq`，其中包含了一个自旋锁和一个引用计数数组，由于`paq`是全局变量，会被自动初始化为全0

这里的计数数组有两种写法：`int paqcon[PHYSTOP / PGSIZE];`或者`int paqcon[PHYSTOP-KERNELBASE / PGSIZE];`。如果选择第二种，之后的地址都需要减去`KERNELBASE`，这里为了方便直接用第一种

```c
// 管理物理页的引用数量
struct
{
  struct spinlock lock;
  int paqcon[PHYSTOP / PGSIZE];
} paq;
```

- 在`kinit`中初始化`paq`的自旋锁

```c++
void kinit()
{
  initlock(&kmem.lock, "kmem");
  // 初始化页引用计数
  initlock(&paq.lock, "paq");
    
  freerange(end, (void *)PHYSTOP);
  memset(paq.paqcon, 0, sizeof(paq.paqcon));
}
```



* 修改`kalloc`和`kfree`函数，在`kalloc`中初始化内存引用计数为1，在`kfree`函数中对内存引用计数减1，如果引用计数为0时才真正删除

```c
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 只有引用为0时才可以释放
  acquire(&paq.lock);
  if (--paq.paqcon[(uint64)pa/PGSIZE] == 0)
  {
    release(&paq.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else {
    release(&paq.lock);
  }
}
```

```c++
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    // 引用设置为 1
    acquire(&paq.lock);
    paq.paqcon[(uint64)r/PGSIZE] = 1;
    release(&paq.lock);
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk


  return (void *)r;
}
```



- 在**`kernel/kalloc.c`**中添加几个辅助函数

```c
// 为指定pa添加一个引用
void paqadd(void* pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return;
  acquire(&paq.lock);
  paq.paqcon[(uint64)pa / PGSIZE] += 1;
  release(&paq.lock);
}

// 返回指定pa的引用数
int paqnum(void* pa) {
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  int num;
  acquire(&paq.lock);
  num = paq.paqcon[(uint64)pa / PGSIZE];
  release(&paq.lock);
  return num;
}


// 判断是否为cow页面
int cowpage(pagetable_t pagetable, uint64 va) {
  if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  return (*pte & COW ? 0 : -1);
}
```

- 修改`freerange`

```c
void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    // 在kfree中将会检查页面的引用数，这里我们设置为1，让页面可以释放
    paq.paqcon[(uint64)p / PGSIZE] = 1; 
    kfree(p);
  }
}
```

- 修改`uvmcopy`，不为子进程分配内存，而是使父子进程共享内存，但禁用`PTE_W`，同时标记`COW`，记得调用`paqadd`增加引用计数

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // 仅对可写页面设置COW标记
    if(flags & PTE_W) {
      // 禁用写并设置COW标记
      flags = (flags | COW) & ~PTE_W;
      *pte = PA2PTE(pa) | flags;
    }

    // 子进程映射到父进程的物理地址
    if(mappages(new, i, PGSIZE, pa, flags) != 0) {
      uvmunmap(new, 0, i / PGSIZE, 1);
      return -1;
    }
    
    // 增加内存的引用计数
    paqadd((char*)pa);
  }
  return 0;
}
```

- 修改`usertrap`，并在`kernel/kalloc.c`中写一个函数处理  ,处理页面错误

```c
// kernel/trap.c
uint64 cause = r_scause();
if(cause == 8) {
  ...
} else if((which_dev = devintr()) != 0){
  // ok
} else if(cause == 13 || cause == 15) {
  uint64 fault_va = r_stval();  // 获取出错的虚拟地址
  if(fault_va >= p->sz
    || cowpage(p->pagetable, fault_va) != 0
    || cowalloc(p->pagetable, PGROUNDDOWN(fault_va)) == 0)
    p->killed = 1;
} else {
  ...
}





// kernel/kalloc.c
// 为cow错误的页面，分配页面
void* cowalloc(pagetable_t pagetable, uint64 va) {
  if(va % PGSIZE != 0)
    return 0;

  uint64 pa = walkaddr(pagetable, va);  // 获取对应的物理地址
  if(pa == 0)
    return 0;

  pte_t* pte = walk(pagetable, va, 0);  // 获取对应的PTE

  if(paqnum((char*)pa) == 1) {
    // 只剩一个进程对此物理地址存在引用
    // 则直接修改对应的PTE即可
    *pte |= PTE_W;
    *pte &= ~COW;
    return (void*)pa;
  } else {
    // 多个进程对物理内存存在引用
    // 需要分配新的页面，并拷贝旧页面的内容
    char* mem = kalloc();
    if(mem == 0)
      return 0;

    // 复制旧页面内容到新页
    memmove(mem, (char*)pa, PGSIZE);

    // 清除PTE_V，否则在mappagges中会判定为remap
    *pte &= ~PTE_V;

    // 为新页面添加映射
    if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~COW) != 0) {
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }

    // 将原来的物理内存引用计数减1
	kfree((char*)PGROUNDDOWN(pa));
    return mem;
  }
}
```

- 在`copyout`中处理相同的情况，如果是COW页面，需要更换`pa0`指向的物理地址

```c
while(len > 0){
  va0 = PGROUNDDOWN(dstva);
  pa0 = walkaddr(pagetable, va0);

  // 处理COW页面的情况
  if(cowpage(pagetable, va0) == 0) {
    // 更换目标物理地址
    pa0 = (uint64)cowalloc(pagetable, va0);
  }

  if(pa0 == 0)
    return -1;

  ...
}
```