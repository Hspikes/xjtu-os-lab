# 操作系统实验仓库（os-lab）

记录 3 轮操作系统课程实验的代码与截图，覆盖用户态进程/线程练习、进程间通信、页面置换模拟，以及内核模块与系统调用表实验。主要语言为 C，Lab3 依赖 Linux 内核开发环境。

## 目录速览
- `1lab/`：基础进程与线程实验，包含 `fork/exec/wait`、信号处理、互斥/信号量、简易自旋锁等示例，`res/` 为运行截图。
- `2lab/`：进阶进程通信与内存管理实验，含 `kill` 信号、管道/锁、`alarm`、自旋锁与互斥、动态内存分配、FIFO/LRU 页面置换模拟器，`res/` 存放输出截图。
- `3lab/`：内核模块、字符设备与系统调用表修改实验，详见 `3lab/README.md`。

## 环境要求
- Linux 开发环境，已安装 `gcc`/`make` 与 `pthread`/`semaphore` 相关头文件。
- Lab3 需要匹配当前内核版本的头文件和 root 权限（加载/卸载模块）。

## 快速开始
用户态实验可直接用 `gcc` 编译运行：
```bash
# 进程/线程示例
gcc 1lab/1/1-1.c -o 1lab/1/1-1
gcc 1lab/2/2-2.c -o 1lab/2/2-2 -pthread
./1lab/2/2-2

# 页面置换模拟器（FIFO/LRU）
gcc 2lab/4/4.c -o 2lab/4/4
./2lab/4/4
```

内核实验请参考子目录内 Makefile：
```bash
cd 3lab/hello_test    && make && sudo insmod mymodules.ko && sudo rmmod mymodules
cd 3lab/dev           && make && sudo insmod glo_pro.ko   && sudo rmmod glo_pro
cd 3lab/modify_syscall&& make && sudo insmod modify_syscall.ko p_sys_call_table=<addr> && sudo rmmod modify_syscall
```
> `p_sys_call_table` 需使用当前内核的实际地址，可通过 `/proc/kallsyms` 查询。更多细节见 `3lab/README.md`。

## 实验小结
- **Lab1**：关注进程创建/等待、父子进程信号、线程同步（互斥锁/信号量/自旋锁），理解基本并发与调度行为。
- **Lab2**：深入信号/管道的同步与竞争、`alarm` 计时、进程/线程并发安全、动态内存与页面置换策略的实现与对比。
- **Lab3**：完成可加载内核模块、字符设备消息队列、系统调用表替换与检查等高级实验，涵盖 CR0 写保护、等待队列、poll/epoll 支持等主题。

## 参考资料
- 各目录内的源码注释与 `res/` 截图可帮助理解运行效果。
- Lab3 的完整步骤、问答与调试技巧见 `3lab/README.md`。***
