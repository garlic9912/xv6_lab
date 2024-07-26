/*
	echo可以写入也可以输出
	echo "Hello World"  ==  输出 Hello World
	echo "Hello World" > file  == 覆盖写入file文件内容"Hello World"
	echo "Hello World" >> file  == 追加写入file文件内容"Hello World"
*/

/*
	$ echo > b  创建一个文件'b'
	$ mkdir a  创建一个目录'a'
	$ echo > a/b  创建一个文件'a/b'
	$ find . b  在当前目录'.'下查找文件'b'
*/

/*
	每个进程都有三个默认打开的文件描述符
	1.标准输入 (stdin): 文件描述符为 0，用于从用户或其他程序接收输入
	2.标准输出 (stdout): 文件描述符为 1，用于向用户或其他程序输出信息
	3.标准错误 (stderr): 文件描述符为 2，用于输出错误信息
*/

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