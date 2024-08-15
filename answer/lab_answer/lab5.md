# 1.Eliminate allocation from sbrk() (难度：easy)

- **题目要求**

> 你的首项任务是删除`sbrk(n)`系统调用中的页面分配代码（位于***sysproc.c***中的函数`sys_sbrk()`）。`sbrk(n)`系统调用将进程的内存大小增加n个字节，然后返回新分配区域的开始部分（即旧的大小）。新的`sbrk(n)`应该只将进程的大小（`myproc()->sz`）增加n，然后返回旧的大小。它不应该分配内存——因此您应该删除对`growproc()`的调用（但是您仍然需要增加进程的大小！）

- **解题步骤**

```c++
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // 增加进程的大小但不分配内存
  myproc()->sz += n;
  return addr;
}
```







# 2.Lazy allocation (难度：moderate)

- **题目要求**

>修改***trap.c***中的代码以响应来自用户空间的页面错误，方法是新分配一个物理页面并映射到发生错误的地址，然后返回到用户空间，让进程继续执行。您应该在生成“`usertrap(): …`”消息的`printf`调用之前添加代码。你可以修改任何其他xv6内核代码，以使`echo hi`正常工作。



- **解题步骤**

这里如果认真写了`lab3`应该不难，利用`kalloc`获得一个物理页，在利用`mappages`错误的虚拟页映射给物理页即可

```c++
void
usertrap(void)
{
  // ...
  } else if (r_scause() == 13 || r_scause() == 15) {
    // 处理页面错误
    uint64 fault_va = r_stval();  // 产生页面错误的虚拟地址
    char* pa;                     // 分配的物理地址
    if(PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz &&
      (pa = kalloc()) != 0) {
        memset(pa, 0, PGSIZE);
        if(mappages(p->pagetable, PGROUNDDOWN(fault_va), PGSIZE, (uint64)pa, PTE_R|PTE_W|PTE_X|PTE_U)!=0) {
          kfree(pa);
          p->killed = 1;
        }
    } else {
      p->killed = 1;
    }
  }

  else if((which_dev = devintr()) != 0){
    // ok
  }
  // ...
}
```

由于是`lazy alloction`页不存在时不应该`panic`，应该`continue`等待分配

```c++
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      continue;
	// ...
}
```







# 3.Lazytests and Usertests (难度：moderate)

- **题目要求**

处理`sbrk()`参数为负的情况。

如果某个进程在高于`sbrk()`分配的任何虚拟内存地址上出现页错误，则终止该进程。

在`fork()`中正确处理父到子内存拷贝。

处理这种情形：进程从`sbrk()`向系统调用（如`read`或`write`）传递有效地址，但尚未分配该地址的内存。

正确处理内存不足：如果在页面错误处理程序中执行`kalloc()`失败，则终止当前进程。

处理用户栈下面的无效页面上发生的错误。





- **解题步骤**

- 处理`sbrk()`参数为负的情况

参数为负数，我们需要注意`p->sz + n > 0`防止虚拟内存小于0

```c++
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc *p = myproc();
  addr = p->sz;
  if(argint(0, &n) < 0)
    return -1;
  if (n >= 0) {
    // 增加大小但不分配内存页
    p->sz += n;
  } else if (p->sz + n > 0) {
    // 减少大小需要减少内存页
    p->sz = uvmdealloc(p->pagetable, p->sz, p->sz + n);
  }
  return addr;
}
```

- 在`fork()`中正确处理父到子内存拷贝。

`uvmcopy()`和`uvmunmap()`中对于`lazy alloction`的页`continue`就行了，不执行`panic`

```c++
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  // ...
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
	// ...
}
```

```c++
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

- 处理这种情形：进程从`sbrk()`向系统调用（如`read`或`write`）传递有效地址，但尚未分配该地址的内存

对于用户态的系统调用，如果向`lazy alloction`的页进行读写的话，必然需要借助`argaddr`，我们对这个函数进行改写。如果发现所需的虚拟地址`lazy alloction`我们分配一个有效的物理内存即可

```c++
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  struct proc* p = myproc();

  // 处理向系统调用传入lazy allocation地址的情况
  if(walkaddr(p->pagetable, *ip) == 0) {
    if(PGROUNDUP(p->trapframe->sp) - 1 < *ip && *ip < p->sz) {
      char* pa = kalloc();
      if(pa == 0)
        return -1;
      memset(pa, 0, PGSIZE);
      // 映射
      if(mappages(p->pagetable, PGROUNDDOWN(*ip), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0) {
        kfree(pa);
        return -1;
      }
    } else {
      return -1;
    }
  }
```

