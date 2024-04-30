#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXSZ 512
// 有限状态自动机状态定义
enum state {
  S_WAIT,         // 等待参数输入，此状态为初始状态或当前字符为空格
  S_ARG,          // 参数内
  S_ARG_END,      // 参数结束
  S_ARG_LINE_END, // 左侧有参数的换行，例如"arg\n"
  S_LINE_END,     // 左侧为空格的换行，例如"arg  \n""
  S_END           // 结束，EOF
};

// 字符类型定义
enum char_type {
  C_SPACE,
  C_CHAR,
  C_LINE_END
};

/**
 * @brief 获取字符类型
 *
 * @param c 待判定的字符
 * @return enum char_type 字符类型
 */
enum char_type get_char_type(char c)
{
  switch (c) {
  case ' ':
    return C_SPACE;
  case '\n':
    return C_LINE_END;
  default:
    return C_CHAR;
  }
}

/**
 * @brief 状态转换
 *
 * @param cur 当前的状态
 * @param ct 将要读取的字符
 * @return enum state 转换后的状态
 */
enum state transform_state(enum state cur, enum char_type ct)
{
  switch (cur) {
  case S_WAIT:
    if (ct == C_SPACE)    return S_WAIT;
    if (ct == C_LINE_END) return S_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  case S_ARG:
    if (ct == C_SPACE)    return S_ARG_END;
    if (ct == C_LINE_END) return S_ARG_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  case S_ARG_END:
  case S_ARG_LINE_END:
  case S_LINE_END:
    if (ct == C_SPACE)    return S_WAIT;
    if (ct == C_LINE_END) return S_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  default:
    break;
  }
  return S_END;
}


/**
 * @brief 将参数列表后面的元素全部置为空
 *        用于换行时，重新赋予参数
 *
 * @param x_argv 参数指针数组
 * @param beg 要清空的起始下标
 */
void clearArgv(char *x_argv[MAXARG], int beg)
{
  // 清空参数列表，为新的参数腾出空间
  for (int i = beg; i < MAXARG; ++i)
    x_argv[i] = 0;
}

int main(int argc, char *argv[])
{
  // 检查输入参数数量是否超过最大限制
  if (argc - 1 >= MAXARG) {
    fprintf(2, "xargs: too many arguments.\n");
    exit(1);
  }
  char lines[MAXSZ]; // 用于存储输入行的缓冲区
  char *p = lines;   // 指向当前处理的字符
  char *x_argv[MAXARG] = {0}; // 参数指针数组，初始化为空

  // 存储原始命令行参数
  for (int i = 1; i < argc; ++i) {
    x_argv[i - 1] = argv[i];
  }
  int arg_beg = 0;          // 参数起始索引
  int arg_end = 0;          // 参数结束索引
  int arg_cnt = argc - 1;   // 当前参数计数
  enum state st = S_WAIT;   // 初始状态

  while (st != S_END) { // 状态机循环，处理输入字符
    // 读取字符，如果读取为空则结束循环
    if (read(0, p, sizeof(char)) != sizeof(char)) {
      st = S_END;
    } else {
      st = transform_state(st, get_char_type(*p));
    }

    if (++arg_end >= MAXSZ) { // 检查参数长度是否超过最大限制
      fprintf(2, "xargs: arguments too long.\n");
      exit(1);
    }

    switch (st) {
    case S_WAIT:          // 等待状态，寻找下一个参数的起始
      ++arg_beg;
      break;
    case S_ARG_END:       // 参数结束，准备处理下一个参数
      x_argv[arg_cnt++] = &lines[arg_beg];
      arg_beg = arg_end;
      *p = '\0';          // 将参数尾部设置为字符串结束符
      break;
    case S_ARG_LINE_END:  // 参数结束且行结束，执行命令
      x_argv[arg_cnt++] = &lines[arg_beg];
      // 操作同S_LINE_END，不重复代码
    case S_LINE_END:      // 行结束，执行当前参数列表的命令
      arg_beg = arg_end;
      *p = '\0';
      if (fork() == 0) { // 创建子进程执行命令
        exec(argv[1], x_argv);
      }
      arg_cnt = argc - 1; // 重置参数计数，准备新的参数列表
      clearArgv(x_argv, arg_cnt); // 清空参数列表
      wait(0); // 等待子进程结束
      break;
    default:
      break;
    }

    ++p;    // 移动到下一个字符位置
  }
  exit(0);
}