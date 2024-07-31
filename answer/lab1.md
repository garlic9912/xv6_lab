# 1.sleep(难度：Easy)

- **题目要求**

```
$ make qemu
...
init: starting sh
$ sleep 10
(nothing happens for a little while)
$
```

在`user/`目录下创建`sleep.c`文件，处理异常情况，加调用sleep系统调用即可





- **解题过程**
- 将 `$U/_sleep` 添加到 Makefile文件 的 `UPROGS` 字段中

```c++
// user/sleep.c
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
  if (argc < 2) {
    // 一个进程默认的三个fd=0,1,2。分别为标准输入，标准输出，标准错误
    fprintf(2, "sleep argument error\n");
    // exit(1):异常退出
    exit(1);
  }
  sleep(atoi(argv[1]));
  // exit(0):正常退出
  exit(0);
}
```

- 操作系统的 shell 解析用户输入，并将命令及其参数传递给相应的程序
- `argc` 表示参数的数量，`argv` 是一个指向参数字符串的指针数组











# 2.pingpong(难度：Easy)

- **题目要求**

```
$ make qemu
...
init: starting sh
$ pingpong
4: received ping
3: received pong
$
```

这题题目大致意思是，命令行输入pingpong后，父进程和子进程传递信息，如果传递成功父进程打印`received pong`，子进程打印`received ping`





- **解题过程**
- 将 `$U/_pingpong` 添加到 Makefile文件 的 `UPROGS` 字段中



创建一个pipe，父进程写入信息，子进程接收信息。父进程wait等待子进程结束。

```c++
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p1[2];  // 父->子
    int p2[2];  // 子->父

    // 创建管道
    // p[0] 是管道的读端，p[1] 是管道的写端
    pipe(p1);
    pipe(p2);

    if (fork() == 0) {
        // 子进程
        char buf[4];
        read(p1[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        write(p2[1], argv[0]+4, 4);
        // 关闭管道
        close(p1[0]);
        close(p2[1]);
        exit(0);
    } else {
        // 父进程
        char buf[4];
        write(p1[1], argv[0], 4);
        wait(0);
        read(p2[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        // 关闭管道
        close(p1[1]);
        close(p2[0]);
        exit(0);
    }
}
```

- 注意管道要关闭，否则会占用资源















# 3.Primes(难度：Moderate/Hard)

- **题目要求**

```
$ make qemu
...
init: starting sh
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
$
```





- **解题过程**
- 将 `$U/_primes` 添加到 Makefile文件 的 `UPROGS` 字段中



这题要借助`Eratosthenes`的筛选法来解题，在第一个进程中筛出2的倍数的数，在第二个进程中筛出3的倍数的数，依次类推。显然要借助递归，所以需要考虑递归的结束条件。

```c++
#include "kernel/types.h"
#include "user/user.h"

void child(int *pl) { 
    // 连接右边的管道
    int prime, count, pr[2];
    // 关闭上一个管道的写端
    close(pl[1]);

    count = read(pl[0], &prime, sizeof(int));
    if (count == 0) exit(0);
    pipe(pr);

    if (fork() == 0) {
        child(pr);
    } else {
        int n;
        close(pr[0]);
        printf("prime %d\n", prime);
        while (read(pl[0], &n, sizeof(int)) != 0) {
            if (n % prime != 0) {
                write(pr[1], &n, sizeof(int));
            }
        }
        close(pl[0]);
        close(pr[1]);
        wait(0);
        exit(0);
    }
    
}



int main(void) {
    int pr[2];
    pipe(pr);

    if (fork() == 0) {
        child(pr);
    } else {
        // 关闭读端
        close(pr[0]);
        for (int i = 2; i <= 35; i++) {
            write(pr[1], &i, sizeof(int));
        }
        // 关闭写端
        close(pr[1]);
        // 等待子进程结束
        wait(0);
    }
    exit(0);
}
```

- 在父进程创建管道后，子进程实际上继承了这个管道的文件描述符。即使父进程关闭了这些描述符，子进程仍然可以使用它们
- 在父子进程中创建一个管道，他实际上有4个文件描述符。父对子的写和读，子对父的写和读，相互之间是独立的
- 对于左边的进程，只需要向右边写入，而不需要读入。所以在左边也就是父进程中要关闭管道的读端，释放资源。右边的进程只需要读入左边进程的数据，而不需要写入，因此可以关闭写端













# 4.find(难度：Moderate)

- **题目要求**

```
$ make qemu
...
init: starting sh
$ echo > b
$ mkdir a
$ echo > a/b
$ find . b
./b
./a/b
$ 
```

-   echo可以写入也可以输出

    echo "Hello World"  ==  输出 Hello World

    echo "Hello World" > file  == 覆盖写入file文件内容"Hello World"  【若是文件不存在则会创建新文件】

    echo "Hello World" >> file  == 追加写入file文件内容"Hello World"



- **解题过程**

把`ls.c`文件的代码看懂就行，然后仿写

```c++
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target_file);

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(2, "ERROR: You need pass in only 2 arguments\n");
		exit(1);
	}
	find(argv[1], argv[2]);
	exit(0);
}

void find(char *path, char *target_file)
{
	int fd;
	struct stat st;
	struct dirent de;
	char buf[512], *p;
	int file_count = 0;

	// 打开文件
	if ((fd = open(path, 0)) < 0)
	{
		fprintf(2, "ERROR: cannot open %s\n", path);
		return;
	}


	// 将当前目录添加到缓冲
	strcpy(buf, path);
	p = buf + strlen(buf);
	*p++ = '/';


	// 读取目录项
	while (read(fd, &de, sizeof(de)) == sizeof(de))
	{

		if (de.inum == 0)
			continue;

		memmove(p, de.name, DIRSIZ);
		p[DIRSIZ] = 0;

		if (stat(buf, &st) < 0)
		{
			fprintf(2, "ERROR: cannot stat %s\n", buf);
		}

		switch (st.type)
		{
		case T_FILE:
			if (strcmp(target_file, de.name) == 0)
			{
				printf("%s\n", buf);
				file_count++;
			}
			break;
		case T_DIR:
			if ((strcmp(de.name, ".") != 0) && (strcmp(de.name, "..") != 0))
			{
				find(buf, target_file);
			}
		}
	}
	close(fd);

	// 没找到文件
	if (file_count == 0) {
		fprintf(2, "ERROR: do not find the given file\n");
	}
	return;
}
```











# 5.xargs(难度：Moderate)

- **题目要求**

```
$ echo hello too | xargs echo bye
bye hello too
$
```

```
$ make qemu
...
init: starting sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $   
```

当你在命令行输入 `echo hello too | xargs echo bye` 时，Shell 会按如下顺序执行：

1. **解析命令行**：Shell 首先会解析整个命令行，将其拆分为 `echo hello too` 和 `xargs echo bye` 两个命令。
2. **创建管道**：Shell 创建一个管道，用来在两个命令之间传递数据。管道由两个文件描述符组成：一个用于写入 (`write`)，一个用于读取 (`read`)。
3. **执行第一个命令 (`echo hello too`)**：
   - Shell 使用 `fork` 创建一个子进程。
   - 在子进程中，Shell 重定向标准输出 (`stdout`) 到管道的写入端。
   - 子进程执行 `echo hello too`，产生输出 `hello too`。
   - 输出通过管道传递到下一个命令。
4. **执行第二个命令 (`xargs echo bye`)**：【这就是我们要写的功能】

- 很显然要从标准输入中读入第一个命令的结果，拼接在`xargs`命令后，创建子进程`exec`运行即可



- **解题过程**

```c++
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

void main(int argc, char *argv[]) {
    int pid, n, buf_index = 0;
    char buf, arg[1024], *args[MAXARG];

    if (argc < 2) {
        fprintf(2, "xargs: need arguments\n");
        exit(0);
    }

    // 读取xargs之后的参数
    for (int i = 1; i < argc; i++) {
        args[i-1] = argv[i];
    }

    // 从标准输入中读取数据
    while ((n = read(0, &buf, 1)) > 0) {
        // 行的结尾
        if (buf == '\n') {
            arg[buf_index] = 0;

            if ((pid = fork()) < 0) {
                fprintf(2, "xargs: fork error\n");
                exit(0);
            } else if (pid == 0) {
                // 拼接
                args[argc-1] = arg;
                args[argc] = 0;
                exec(args[0], args);
            } else {
                wait(0);
                buf_index = 0;
            }
        } 
        else 
            arg[buf_index++] = buf;
    }
    exit(0);
}
```

