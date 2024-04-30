/*
 * find: 在指定目录及其子目录中查找指定文件
 * 参数:
 *    path: 指定的起始查找路径
 *    filename: 需要查找的文件名
 * 注意:
 *    该函数会递归查找指定目录及其子目录下的文件，如果找到匹配的文件则打印其完整路径。
 */

#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char *path, const char *filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 尝试打开指定的路径，如果不是目录则报错
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // 获取路径的状态信息，用于判断是否为目录
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot fstat %s\n", path);
    close(fd);
    return;
  }

  // 检查路径是否为目录，如果不是则打印用法信息
  if (st.type != T_DIR) {
    fprintf(2, "usage: find <DIRECTORY> <filename>\n");
    return;
  }

  // 检查构建文件路径的缓冲区是否足够大
  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long\n");
    return;
  }
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/'; // 准备拼接文件名

  // 遍历目录中的所有条目
  while (read(fd, &de, sizeof de) == sizeof de) {
    if (de.inum == 0)
      continue; // 忽略空目录项
    memmove(p, de.name, DIRSIZ); // 将文件名添加到路径缓冲区
    p[DIRSIZ] = 0; // 确保路径是终止的字符串

    // 获取条目状态，用于进一步判断
    if (stat(buf, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }

    // 递归查找子目录中的文件，或检查当前条目是否匹配目标文件名
    if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0) {
      find(buf, filename);
    } else if (strcmp(filename, p) == 0) {
      printf("%s\n", buf); // 找到匹配的文件，打印其路径
    }
  }

  close(fd); // 关闭目录文件描述符
}

int main(int argc, char *argv[])
{
  // 检查命令行参数数量是否正确
  if (argc != 3) {
    fprintf(2, "usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]); // 调用find函数进行查找
  exit(0);
}