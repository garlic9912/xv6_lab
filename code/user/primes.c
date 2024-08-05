/*
    1.父进程创建的管道，子进程是可以继承使用的
    2.父进程关闭了管道的读写端只是关闭了父进程对于管道的访问，无关子进程，所以子进程依然可以访问管道
    3.因此推荐：父进程完成任务后立刻关闭管道读写端，节省资源
*/

#include "kernel/types.h"
#include "user/user.h"

void child(int *pl) {
    
    // 连接右边的管道
    int prime, count, pr[2];
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
        close(pr[0]);
        for (int i = 2; i <= 35; i++) {
            write(pr[1], &i, sizeof(int));
        }
        close(pr[1]);
        wait(0);
    }
    exit(0);
}

