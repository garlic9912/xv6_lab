#include "kernel/types.h"
#include "user/user.h"

// shell写入command后，会进行解析
// argc是参数的数量
// argv[]的每个元素按序存放分离的参数, 第一个argv[0]是调用函数的名字
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "The argument is wrong\n");
        // exit(1): 表示程序遇到错误并异常退出。非零的状态码（如 1）通常用于指示程序在执行过程中遇到某种错误
        exit(1);
    }
    // 系统调用
    sleep(atoi(argv[1]));
    // exit(0): 表示程序成功执行并正常退出，状态码 0 通常用于指示程序没有遇到任何错误
    exit(0);
}