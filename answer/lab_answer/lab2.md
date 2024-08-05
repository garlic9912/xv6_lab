# 1.System call tracing（难度：moderate）

- **题目要求**

实验要求编写trace函数系统调用，实现如下功能

```
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
```

​	trace函数有一个参数，是一个`uint`类型。32是`1<<SYS_read`，147483647将所有31个低位置为1。即根据对应二进制位上0或1判断跟踪具体那个系统调用，右移的位数由系统调用的编号确定。

​	`grep`是Unix命令，作用是在文件中使用正则表达式搜索文本，并把匹配的行打印出来





- **解题过程**

- 将 `$U/_trace` 添加到 Makefile文件 的 `UPROGS` 字段中



- `user/trace.c`文件已给出,  其主要的代码如下

```c++
if (trace(atoi(argv[1])) < 0) {
  fprintf(2, "%s: trace failed\n", argv[0]);
  exit(1);
}
  
for(i = 2; i < argc && i < MAXARG; i++){
  nargv[i-2] = argv[i];
}
exec(nargv[0], nargv);
```

使用了trace系统调用，执行后续命令，并跟踪



- 系统调用trace并未声明，为了启动`qemu`需要声明trace系统调用

  - 在 `user/user.h` 文件中加入函数声明：`int trace(int);`

  - 同时，为了生成进入中断的汇编文件，需要在 `user/usys.pl` 添加进入内核态的入口函数的声明：`entry("trace");`，以便使用 `ecall` 中断指令进入内核态

  - 同时在 `kernel/syscall.h` 中添加系统调用编号，编号顺延即可



- 实现 sys_trace() 函数，在 `kernel/sysproc.c` 文件中。以及在每个 trace 进程中，添加一个 mask 字段，用来识别是否执行了 mask 标记的系统调用。在执行 trace 进程时，如果进程调用了 mask 所包括的系统调用，就打印到标准输出中

```c++
// kernel/proc.h
struct proc {
  //...
  int mask;                    // trace的参数-掩码
};
```

```c++
// kernel/sysproc.c
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
```



- 改写`fork()`函数以跟踪子进程

```c++
// kernel/proc.c
int
fork(void)
{
  //...
  np->mask = p->mask;
}
```



- 所有的系统调用都需要通过 `kernel/syscall.c` 中的 `syscall()` 函数来执行，因此需要改写此函数
- 添加系统调用入口 `extern uint64 sys_trace(void);` 和 `[SYS_trace] sys_trace,`到 `kernel/syscall.c` 中
- 在`usys.S`文件中可以看到系统调用的编号被写入了`a7`寄存器
- `p->trapframe->a0` 存储的了函数调用的返回值

```c++
void
syscall(void)
{
  int num;
  struct proc *p = myproc();
 
  // 获取系统调用号
  num = p->trapframe->a7;
  // 检查系统调用号的有效性，并调用相应的系统调用处理函数
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
    // 筛选trace系统调用, 并打印跟踪信息
    if ((1 << num) & p->mask) {
      printf("%d: syscall %s -> %d\n", p->pid, syscalls_name[num], p->trapframe->a0);
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

- 由于需要打印系统调用的名称，但此时我们只要系统调用编号，因此需要一个静态数组。通过系统调用编号获得名称

```c++
static char *syscalls_name[] = {
[SYS_fork]    "fork",
[SYS_exit]    "exit",
[SYS_wait]    "wait",
[SYS_pipe]    "pipe",
[SYS_read]    "read",
[SYS_kill]    "kill",
[SYS_exec]    "exec",
[SYS_fstat]   "fstat",
[SYS_chdir]   "chdir",
[SYS_dup]     "dup",
[SYS_getpid]  "getpid",
[SYS_sbrk]    "sbrk",
[SYS_sleep]   "sleep",
[SYS_uptime]  "uptime",
[SYS_open]    "open",
[SYS_write]   "write",
[SYS_mknod]   "mknod",
[SYS_unlink]  "unlink",
[SYS_link]    "link",
[SYS_mkdir]   "mkdir",
[SYS_close]   "close",
[SYS_trace]   "trace",
};
```









# 2.Sysinfo（难度：moderate）

- **题目要求**

>在这个作业中，您将添加一个系统调用`sysinfo`，它收集有关正在运行的系统的信息。系统调用采用一个参数：一个指向`struct sysinfo`的指针（参见***kernel/sysinfo.h***）。内核应该填写这个结构的字段：`freemem`字段应该设置为空闲内存的字节数，`nproc`字段应该设置为`state`字段不为`UNUSED`的进程数。我们提供了一个测试程序`sysinfotest`；如果输出“**sysinfotest: OK**”则通过。





- **解题步骤**

- 在`Makefile`的`UPROGS`中添加`$U/_sysinfotest`



- 解决`make qemu`编译报错的问题
  - user/user.h 文件中添加新的系统调用`int sysinfo(struct sysinfo *);`， 记得添加对应的结构体声明
  - user/usys/pl 文件中添加`entry("sysinfo");`
  - kernel/syscall.h 文件中添加`sysinfo`的系统调用编号
  - 添加系统调用入口 `extern uint64 sys_trace(void);` 和 `[SYS_trace] sys_trace,`到 `kernel/syscall.c` 中

这下子make qemu就可以正常编译了，但是内核中`sysinfo`的内容还没写所以执行会失败



- 阅读并学习`kernel/kalloc.c`中的代码，并编写一个函数获得空闲内存量

`kernel/kalloc.c`实现了一个简单的物理内存分配器，用于管理物理内存页的分配和释放

每个物理页大小为 4096 字节（即 4KB）

通过链表管理空闲页，实现了内核中对物理内存的基本管理

```c++
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

  return n*PGSIZE;
}
```







- 阅读并学习`kernel/proc.c`中的代码，并编写一个函数获得进程数

主要看结构体`struct proc proc[NPROC];`和函数`static struct proc* allocproc(void)`

```c++
// 获取state不为UNUSED的进程数
uint64
get_proc(void) {
  uint64 n = 0;
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state != UNUSED) {
      n++;
    }
  }
  return n;
}
```







- 阅读并学习`kernel/file.c`中`int filestat`的代码，学习如何使用`copyout()`函数

这个 `copyout` 函数将数据从内核空间复制到用户空间。将一段长度为 `len` 的数据从内核的源地址 `src` 复制到用户虚拟地址空间中的 `dstva`

```c++
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
```

