# 1.Uthread: switching between threads (难度：moderate)

- **题目要求**

在本练习中，您将为用户级线程系统设计上下文切换机制，然后实现它。为了让您开始，您的xv6有两个文件：***user/uthread.c***和***user/uthread_switch.S***，以及一个规则：运行在***Makefile***中以构建`uthread`程序。***uthread.c***包含大多数用户级线程包，以及三个简单测试线程的代码。线程包缺少一些用于创建线程和在线程之间切换的代码。

您需要将代码添加到***user/uthread.c***中的`thread_create()`和`thread_schedule()`，以及***user/uthread_switch.S***中的`thread_switch`。一个目标是确保当`thread_schedule()`第一次运行给定线程时，该线程在自己的栈上执行传递给`thread_create()`的函数。另一个目标是确保`thread_switch`保存被切换线程的寄存器，恢复切换到线程的寄存器，并返回到后一个线程指令中最后停止的点。您必须决定保存/恢复寄存器的位置；修改`struct thread`以保存寄存器是一个很好的计划。您需要在`thread_schedule`中添加对`thread_switch`的调用；您可以将需要的任何参数传递给`thread_switch`，但目的是将线程从`t`切换到`next_thread`。



- **解题步骤**

为每个线程添加上下文

```c++
// 线程上下文
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;       /* 用于线程上下文的切换 */
};
```



在`thread_schedule`中，我们要切换`current_thread`和`next_thread`

```c++
void 
thread_schedule(void)
{
	//...
    
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)&t->context, (uint64)&current_thread->context);
  } else
    next_thread = 0;
}
```



在`thread_switch`，我们需要保存第一个参数也就是`current_thread`的上下文，并且加载`next_thread`的上下文

代码可以参考`kernel/swtch.S`

```assembly
thread_switch:
	/* YOUR CODE HERE */
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)	
	ret    /* return to ra */

```



在`thread_create`中设定返回地址和栈指针

```c++
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  
  // 设定返回地址
  t->context.ra = (uint64)func;   
  // 设定栈指针                
  t->context.sp = (uint64)t->stack + STACK_SIZE;  
}
```







# 2.Using threads (难度：moderate)

- **题目要求**

为了避免这种事件序列，请在***notxv6/ph.c***中的`put`和`get`中插入`lock`和`unlock`语句，以便在两个线程中丢失的键数始终为0。相关的pthread调用包括：

- `pthread_mutex_t lock;            // declare a lock`
- `pthread_mutex_init(&lock, NULL); // initialize the lock`
- `pthread_mutex_lock(&lock);       // acquire lock`
- `pthread_mutex_unlock(&lock);     // release lock`

当`make grade`说您的代码通过`ph_safe`测试时，您就完成了，该测试需要两个线程的键缺失数为0。在此时，`ph_fast`测试失败是正常的。

不要忘记调用`pthread_mutex_init()`。首先用1个线程测试代码，然后用2个线程测试代码。您主要需要测试：程序运行是否正确呢（即，您是否消除了丢失的键？）？与单线程版本相比，双线程版本是否实现了并行加速（即单位时间内的工作量更多）？

在某些情况下，并发`put()`在哈希表中读取或写入的内存中没有重叠，因此不需要锁来相互保护。您能否更改***ph.c***以利用这种情况为某些`put()`获得并行加速？提示：每个散列桶加一个锁怎么样？



- **解题步骤**

根据提示，我们可以为每个桶声明一个锁

```c++
// 声明锁
pthread_mutex_t lock[NBUCKET];
```

```c++
int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  // 初始化锁
  for (int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&lock[i], NULL);
  }
    
  // ...
}
```



对于两个线程，他们会相互竞争。在`insert`中对桶添加新的键值对时，如果发生了线程的切换，会导致`key`的缺失。所以需要在`insert`时加锁，为对应的桶加锁

```c++
static 
void put(int key, int value)
{
  int i = key % NBUCKET;
	// ...
    e->value = value;
  } else {
    pthread_mutex_lock(&lock[i]);     // 加锁
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);   // 解锁
  }
}
```









# 3.Barrier(难度：moderate)

- **题目要求**

在本作业中，您将实现一个[屏障](http://en.wikipedia.org/wiki/Barrier_(computer_science))（Barrier）：应用程序中的一个点，所有参与的线程在此点上必须等待，直到所有其他参与线程也达到该点。您将使用pthread条件变量，这是一种序列协调技术，类似于xv6的`sleep`和`wakeup`。

您应该在真正的计算机（不是xv6，不是qemu）上完成此任务。

文件***notxv6/barrier.c***包含一个残缺的屏障实现。

```
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```



2指定在屏障上同步的线程数（***barrier.c***中的`nthread`）。每个线程执行一个循环。在每次循环迭代中，线程都会调用`barrier()`，然后以随机微秒数休眠。如果一个线程在另一个线程到达屏障之前离开屏障将触发断言（assert）。期望的行为是每个线程在`barrier()`中阻塞，直到`nthreads`的所有线程都调用了`barrier()`。





- **解题步骤**

进入`barrier`时`nthread`加一，如果等于给定的线程数，则表明所有线程都已停止在此处。因此释放所有睡眠的线程

如果有线程没有达到，则阻塞该线程，`nthread`加一

```c++
static void 
barrier()
{
  pthread_mutex_lock(&bstate.barrier_mutex);      

  if (++bstate.nthread == nthread) {
    // 释放线程
    bstate.round++;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);    
  } else {
    // 阻塞线程
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  pthread_mutex_unlock(&bstate.barrier_mutex); 
}
```

