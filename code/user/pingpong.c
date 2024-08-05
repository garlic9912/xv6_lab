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