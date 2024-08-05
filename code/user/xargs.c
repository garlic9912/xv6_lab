// EOF是当文件读到结尾时返回的标志(-1)




/*
当你运行 sh < xargstest.sh 时，系统会执行以下步骤：
1.打开 xargstest.sh 文件：系统打开文件 xargstest.sh 并将其内容作为输入。
2.启动 sh：系统启动一个新的 sh shell 进程。
3.输入重定向：通过 < 符号，xargstest.sh 文件的内容被作为输入提供给 sh shell，而不是默认的键盘输入。
4.执行脚本内容：sh shell 读取 xargstest.sh 文件的内容并按顺序执行其中的每一条命令。
*/




/*
当你在命令行输入 echo hello too | xargs echo bye 时，Shell 会按如下顺序执行：

1.解析命令行：Shell 首先会解析整个命令行，将其拆分为 echo hello too 和 xargs echo bye 两个命令。

2.创建管道：Shell 创建一个管道，用来在两个命令之间传递数据。管道由两个文件描述符组成：一个用于写入 (write)，一个用于读取 (read)。

3.执行第一个命令 (echo hello too)：
    Shell 使用 fork 创建一个子进程。
    在子进程中，Shell 重定向标准输出 (stdout) 到管道的写入端。
    子进程执行 echo hello too，产生输出 hello too。
    输出通过管道传递到下一个命令。

4.执行第二个命令 (xargs echo bye)：
    Shell 使用 fork 创建另一个子进程。
    在子进程中，Shell 重定向标准输入 (stdin) 到管道的读取端。
    子进程执行 xargs echo bye。
    xargs 读取来自管道的输入，即 hello too。
    xargs 将读取的输入拆分成独立的参数。
    xargs 将参数传递给后面的命令，即 echo bye。
    xargs 以如下方式执行 echo：echo bye hello too。
    输出结果为：bye hello too。
*/


// 读取输入：使用 read(0, buffer, size) 函数来从标准输入（文件描述符 0）读取数据。
// 写入输出：使用 write(1, buffer, size) 函数来向标准输出（文件描述符 1）写入数据。


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
            // 形成字符串
            arg[buf_index] = 0;

            if ((pid = fork()) < 0) {
                fprintf(2, "xargs: fork error\n");
                exit(0);
            } else if (pid == 0) {
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