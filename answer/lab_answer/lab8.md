# 1.Memory allocator(难度：moderate)

- **题目要求**

> 您的工作是实现每个CPU的空闲列表，并在CPU的空闲列表为空时进行窃取。所有锁的命名必须以“`kmem`”开头。也就是说，您应该为每个锁调用`initlock`，并传递一个以“`kmem`”开头的名称。运行`kalloctest`以查看您的实现是否减少了锁争用。要检查它是否仍然可以分配所有内存，请运行`usertests sbrkmuch`。您的输出将与下面所示的类似，在`kmem`锁上的争用总数将大大减少，尽管具体的数字会有所不同。确保`usertests`中的所有测试都通过。评分应该表明考试通过。



- **解题步骤**

创建一个字符指针数组，用于给`cpu`的`kmem`锁加名字

将`kmem`改成结构体数组，一个`cpu`对应一个`lock`和`freelist`并在`kinit()`中为锁初始化

```c++
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
```



获取当前cpu的id号

```c++
// 获取cpu的id号
int getCpuId() {
  return cpuid();
}
```





对`kalloc`和`kfree`函数改造，每个`cpu`使用自己的`freelist`

并且在`kalloc`中会遇到空闲列表没有空闲页的情况，需要单独写一个函数，从其他`cpu`的空闲列表中窃取空闲页

```c++
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
```





函数返回一个从其他`cpu`空闲列表获得的一个空闲页

```c++
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
```

