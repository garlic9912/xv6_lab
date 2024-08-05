# 1.Print a page table (难度：easy)

- **题目要求**

>定义一个名为`vmprint()`的函数。它应当接收一个`pagetable_t`作为参数，并以下面描述的格式打印该页表。在`exec.c`中的`return argc`之前插入`if(p->pid==1) vmprint(p->pagetable)`，以打印第一个进程的页表。如果你通过了`pte printout`测试的`make grade`，你将获得此作业的满分。

```c++
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
```

第一行显示`vmprint`的参数。之后的每行对应一个PTE，包含树中指向页表页的PTE。每个PTE行都有一些“`..`”的缩进表明它在树中的深度。每个PTE行显示其在页表页中的PTE索引、PTE比特位以及从PTE提取的物理地址。不要打印无效的PTE。在上面的示例中，顶级页表页具有条目0和255的映射。条目0的下一级只映射了索引0，该索引0的下一级映射了条目0、1和2。

您的代码可能会发出与上面显示的不同的物理地址。条目数和虚拟地址应相同。





- **解题步骤**
- 先来看看虚拟内存如何通过页表转换成物理内存

![img](../images/lab3_01.png)

虚拟内存可以分成三个部分，第一部分低12位是页内偏移量。第二部分13~39位是index位，这个index是一个逻辑地址，他指向的是页表内的第几个页表项`(PTE)`，通过页表项我们可以获得物理地址的高44位`PPN`，这个和页内偏移量结合就是内存中的物理地址。第三部分是EXT暂时没有太大用处

![img](../images/lab3_02.png)

这张图片是一个三级页表的结构，三级页表的结构大大缓解了页表过长而占用很大连续地址空间的缺点，并且可扩展性很强。

这里虚拟地址被分为了五个部分，`L2`，`L1`，`L0`，页内偏移量。后面三个都是逻辑地址，分别指明了一级页表的一个页表项，二级页表的一个页表项，三级页表的一个页表项。一级页表的物理地址存在一个寄存器中可以直接取出，借助`L2`可以获得一个物理地址，也就是二级页表的物理位置。`L1`和二级页表找打一级页表，`L0`和一级页表找到最终的物理地址高位，结合`Offset`获得最终的物理地址

**可以看做一个树，一级页表和二级页表都是非叶子结点，三级页表是叶子结点**



- 阅读`kernel/vm.c`和`kernel/riscv.h`

在`riscv.h`中定义了一些宏定义，方便代码编写

```c++
#define PTE_V (1L << 0) // 该标志位表示页表项是否有效
#define PTE_R (1L << 1) // 该标志位表示页面可读
#define PTE_W (1L << 2) // 该标志位表示页面可写
#define PTE_X (1L << 3) // 该标志位表示页面可执行
#define PTE_U (1L << 4) // 该标志位表示用户级代码是否可以访问页面如果设置了这个标志位，用户级代码可以访问；否则不能访问

#define PTE2PA(pte) (((pte) >> 10) << 12) // 获取页表项中的高位物理地址，也就是把后面的flags(看图)去掉
```



在`vm.c`文件中的`freewalk()`函数帮助理解多级页表的遍历过程

```c++
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
```





- 代码编写

```c++
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
```

- 因为题目要求要打印`vmprint`的参数，为了解决递归过程多次打印参数的问题，需要一个辅助函数来进行递归
- 辅助函数采用了原函数加下划线。下划线表示这个函数是内部使用的，它不应该被外部代码直接调用，而是由模块或库的内部实现使用。这是一种约定，用来提醒开发者不要在模块或类外部直接调用这些函数