# System call tracing（moderate）

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

