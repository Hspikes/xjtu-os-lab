# Lab3 实验手册（内核模块、字符设备、系统调用表）

> 本 README 旨在复习与讲解 Lab3 的全部内容，涵盖每个子目录的代码说明、编译运行步骤、涉及的内核概念、常见问题与答辩提示。全文以中文撰写，便于课堂讲解与自查。

---
## 目录
1. 总览与目标
2. 代码结构
3. 环境与准备
4. Hello 模块（hello_test）
5. 字符设备消息队列（dev）
   - 数据结构与同步
   - 读写流程
   - poll/epoll 支持
   - 用户态测试程序
6. 系统调用劫持实验（modify_syscall）
   - 原理与安全注意
   - 关键代码解析
   - 用户态验证程序
7. sys_call_table 检查模块（check）
8. 编译、加载、卸载步骤
9. 常见问题与调试技巧
10. 课堂/验收问答要点
11. 术语小抄

---
## 1. 总览与目标
Lab3 聚焦 Linux 内核编程的三个典型主题：
- **内核模块基础**：理解模块的加载、卸载流程与日志输出。
- **字符设备驱动**：实现一个内核态消息队列，支持阻塞/非阻塞读写与 poll。
- **系统调用表修改**：学习 sys_call_table 劫持机制、CR0 写保护位、恢复原入口。

通过这些实验，可以综合练习：
- 内核 API 的使用（module_init/exit、cdev、wait_queue、copy_to/from_user 等）。
- 同步与并发控制（mutex、等待队列、原子变量）。
- 内核地址与安全性（sys_call_table、CR0 WP 位）。

---
## 2. 代码结构
- `hello_test/`：最小可加载模块示例。
- `dev/`：字符设备 `/dev/m2mchardev` 及其用户态读写测试程序。
- `modify_syscall/`：系统调用劫持模块与两个验证程序。
- `check/`：检查 sys_call_table[96] 当前入口的 proc 模块。
- `.vscode/`：编辑器配置（与实验无关）。

---
## 3. 环境与准备
1) 需要匹配的 Linux 内核版本与头文件（通常在同一台实验机上编译运行）。  
2) 需要 root 权限加载模块：`sudo insmod ...` / `sudo rmmod ...`。  
3) 获取 `sys_call_table` 地址：可通过 `sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms`，不同内核版本会变化。  
4) 建议在虚拟机或测试机上操作，劫持系统调用存在风险。  

---
## 4. Hello 模块（hello_test）
文件：`hello_test/mymodules.c`

- **核心逻辑**：定义 `my_module_init` 与 `my_module_exit`，在加载/卸载时 `printk` 一条日志。
- **关键点**：
  - `module_init/exit` 宏注册入口/出口。
  - `MODULE_LICENSE("GPL")` 避免内核提示许可证不兼容。
  - `printk` 日志可用 `dmesg | tail` 查看。
- **用途**：验证构建环境、理解模块生命周期。

示例操作：
```bash
cd hello_test
make
sudo insmod mymodules.ko
dmesg | tail
sudo rmmod mymodules
dmesg | tail
```

---
## 5. 字符设备消息队列（dev）
核心文件：`dev/glo_pro.c`  
用户态测试：`dev/read.c`、`dev/write.c`

### 5.1 设计目标
实现一个“内核态消息队列”字符设备 `/dev/m2mchardev`，允许多个进程/线程：
- 向队列写入消息（有总容量和单消息长度限制）。
- 从队列读取消息（FIFO）。
- 支持阻塞/非阻塞模式与 `poll/epoll`。

### 5.2 主要常量
- `M2M_DEVICE_NAME = "m2mchardev"`：设备名。
- `MAX_MSG = 4 * 1024`：单条消息最大 4KB。
- `MAX_BYTES = 64 * 1024`：队列累计字节上限 64KB。

### 5.3 内部数据结构
- `struct msg`：链表节点，包含 `len` 与 `data` 指针。
- `struct mydev`：设备全局状态
  - `struct cdev devm`：字符设备对象。
  - `struct list_head msg_list`：消息链表。
  - `size_t queued_bytes`：当前队列字节总数。
  - `struct mutex lock`：保护队列与长度。
  - `wait_queue_head_t readq/writeq`：阻塞读/写的等待队列。
  - `atomic_t readers/writers`：统计打开的读/写端数量。
- `struct client_ctx`：每个打开文件的上下文，记录读写角色（本实验默认均可读写）。

### 5.4 关键函数与流程
- `msg_alloc/msg_free`：为消息分配与释放内核内存（`kmalloc`）。
- `m2m_open`：
  - 分配 `client_ctx`，默认读写均可。
  - `atomic_inc` 维护读写计数。
- `m2m_release`：释放 `client_ctx`，递减计数。
- `m2m_read`：
  - 非阻塞且队列空：返回 `-EAGAIN`。
  - 阻塞读取：`wait_event_interruptible(readq, !list_empty(...))`。
  - 取队首节点，`copy_to_user`，释放节点，更新 `queued_bytes`，唤醒写等待队列。
- `m2m_write`：
  - 检查单条消息大小（>MAX_MSG 返回 `-EMSGSIZE`）。
  - 若写入后将超出 `MAX_BYTES`：
    - 非阻塞：返回 `-EAGAIN`。
    - 阻塞：`wait_event_interruptible(writeq, queued_bytes + count <= MAX_BYTES)`。
  - `copy_from_user` 数据，尾插链表，更新 `queued_bytes`，唤醒读等待队列。
- `m2m_poll`：
  - 将读写等待队列加入 `poll_wait`。
  - 队列非空 -> `POLLIN|POLLRDNORM`；队列未满 -> `POLLOUT|POLLWRNORM`。

### 5.5 设备注册/注销
- 初始化：`alloc_chrdev_region`（或 `register_chrdev_region`）、`cdev_add`、`class_create`、`device_create`。
- 卸载：逆序释放设备、类、cdev、chrdev_region，并清空残留消息。

### 5.6 用户态测试程序
- `write.c`：
  - `open("/dev/m2mchardev", O_RDWR)`，从 stdin 读入行写入设备。
  - 可并发启动多个写端，验证队列与阻塞。
- `read.c`：
  - `open` 后循环 `read` 1024 字节并打印。
  - 若读到 `"quit"` 则退出。

### 5.7 编译与运行示例
```bash
cd dev
make
sudo insmod glo_pro.ko
./write &   # 写端
./read      # 读端
# 在写端输入多行文本，读端会按消息输出
sudo rmmod glo_pro
```

### 5.8 讨论点
- 为什么需要 `mutex` 而不是 `spinlock`？（涉及睡眠的场景：copy_to/from_user，wait_event，会主动调度）
- 阻塞与非阻塞的差别：依据 `O_NONBLOCK` 与返回 `-EAGAIN`。
- poll/epoll 的事件触发条件与边缘场景（例如写入后立即可读）。

---
## 6. 系统调用劫持实验（modify_syscall）
核心文件：`modify_syscall/modify_syscall.c`  
用户态验证：`modify_old_syscall.c`、`modify_new_syscall.c`

### 6.1 实验思路
1. 找到 `sys_call_table` 的内核地址（运行态或 System.map）。  
2. 关闭 CR0 写保护位 `WP_BIT`，暂时允许写入内核只读区域。  
3. 将 `sys_call_table[96]`（原 gettimeofday）替换为自定义函数 `hello(a,b)`。  
4. 卸载模块时恢复原始表项，避免系统失效。  

### 6.2 关键代码
- `clear_wp/restore_wp`：通过内联汇编读写 CR0，清除/恢复写保护位。
- `p_sys_call_table`：硬编码或通过模块参数传入；计算表项地址 `p_sys_call_table + SYS_NO * sizeof(void *)`。
- `hello(a,b)`：新系统调用实现，返回两数之和，并 `printk` 记录调用。
- `modify_syscall`：保存旧入口 `old_sys_call_func`，写入新入口。
- `restore_syscall`：卸载时写回旧入口，确保系统恢复。

### 6.3 安全注意
- 地址错误会直接导致内核崩溃（panic）。必须使用正确内核版本的地址。
- 关闭写保护是危险操作，仅限实验环境。
- 模块参数要传递 `p_sys_call_table`，避免硬编码失效。

### 6.4 用户态验证程序
- `modify_old_syscall.c`：在未替换前，`syscall(96, &tv, NULL)` 获取时间。
- `modify_new_syscall.c`：替换后调用 `syscall(96, 10, 20)` 期望输出 30。

### 6.5 操作示例
```bash
cd modify_syscall
make
# 获取地址：sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms
sudo insmod modify_syscall.ko p_sys_call_table=0xffffffffXXXXXXXX
./modify_new_syscall    # 输出 a+b
sudo rmmod modify_syscall
```

---
## 7. sys_call_table 检查模块（check）
文件：`check/check_syscall_entry.c`

- 模块参数 `p_sys_call_table` 指定表地址。
- 创建 `/proc/syscall96`，显示：
  - 当前传入的表地址。
  - `sys_call_table[96]` 的实际入口地址。
- 用途：在劫持前/后检查表项是否被替换成功。

示例：
```bash
cd check
make
sudo insmod check_syscall_entry.ko p_sys_call_table=0xffffffffXXXXXXXX
cat /proc/syscall96
sudo rmmod check_syscall_entry
```

---
## 8. 编译、加载、卸载步骤（汇总）
### 8.1 编译
```bash
make          # 在各自子目录运行
```
### 8.2 加载/卸载
- Hello 模块：
  ```bash
  sudo insmod hello_test/mymodules.ko
  sudo rmmod mymodules
  ```
- 字符设备：
  ```bash
  sudo insmod dev/glo_pro.ko
  sudo rmmod glo_pro
  ```
- 系统调用劫持：
  ```bash
  sudo insmod modify_syscall/modify_syscall.ko p_sys_call_table=0xffffffffXXXXXXXX
  sudo rmmod modify_syscall
  ```
- 检查模块：
  ```bash
  sudo insmod check/check_syscall_entry.ko p_sys_call_table=0xffffffffXXXXXXXX
  sudo rmmod check_syscall_entry
  ```

---
## 9. 常见问题与调试技巧
1) **insmod 提示未知符号或内核版本不匹配**  
   - 确认使用的内核头文件与当前运行内核一致。
2) **设备节点不存在**  
   - 检查 `device_create` 是否成功；必要时手动 `mknod /dev/m2mchardev c <major> 0`。
3) **读写阻塞不返回**  
   - 检查是否以 `O_NONBLOCK` 打开；确认是否有另一端写/读；查看 `queued_bytes` 是否超过上限导致写阻塞。
4) **劫持后系统崩溃**  
   - 立即重启；确认 `p_sys_call_table` 地址正确；卸载前恢复原表项。
5) **proc 文件无输出或权限不足**  
   - 确认模块已加载且地址参数正确；以 root 读取 `/proc/syscall96`。
6) **dmesg 太多信息**  
   - 使用 `dmesg | tail -n 50`，或 `dmesg -w` 实时查看。

---
## 10. 课堂/验收问答要点
### 10.1 模块基础
- 问：`module_init` 与 `module_exit` 作用是什么？  
  答：注册模块加载与卸载时的入口函数，负责资源申请与释放。
- 问：`MODULE_LICENSE` 为什么重要？  
  答：声明许可证，避免内核标记为“tainted”，并允许使用 GPL-only 符号。

### 10.2 字符设备
- 问：`copy_to_user/copy_from_user` 为什么必须使用？  
  答：用户态指针不能直接解引用，需借助内核提供的安全拷贝接口。
- 问：如何处理阻塞与非阻塞？  
  答：检查 `O_NONBLOCK`，非阻塞直接返回 `-EAGAIN`，阻塞使用 `wait_event_interruptible`。
- 问：`poll` 如何判断可读/可写？  
  答：队列非空 -> `POLLIN`，队列未满 -> `POLLOUT`。
- 问：为什么使用 `mutex` 而不是 `spinlock`？  
  答：读写路径可能睡眠（等待队列、copy_to/from_user），需可睡眠锁。

### 10.3 系统调用表
- 问：`sys_call_table` 在哪里？  
  答：地址可通过 `/proc/kallsyms` 或 `System.map` 获取，版本相关。
- 问：为什么要清除 CR0 的 WP 位？  
  答：`sys_call_table` 在只读内存，需暂时关闭写保护才能修改。
- 问：如何恢复？  
  答：卸载模块时写回原表项，恢复 WP 位，避免系统异常。
- 问：风险有哪些？  
  答：地址错误或并发调用可能导致内核崩溃，实际生产环境禁用此类操作。

---
## 11. 术语小抄
- **`cdev`**：字符设备核心结构，绑定 `file_operations`。
- **`file_operations`**：定义 open/read/write/poll 等回调。
- **`wait_event_interruptible`**：在条件满足前睡眠，信号可中断。
- **`copy_to_user/from_user`**：用户态与内核态缓冲区安全拷贝。
- **`sys_call_table`**：系统调用号到函数指针的跳转表。
- **`CR0.WP`**：写保护位，禁止写入内核只读页。
- **`poll/epoll`**：多路复用接口，检测 fd 可读/可写等事件。

---
## 12. 进一步探索建议
- 为 m2m 设备增加统计接口（如通过 procfs/seq_file 展示当前队列长度、累计写入次数）。
- 增加权限控制：仅允许写端或读端特定用户访问（通过 `open` 标志位或 ioctl）。
- 在劫持实验中，尝试更安全的方式，例如 ftrace/kprobe/bpf，而非直接改 sys_call_table。

---
## 13. 行为准则与提醒
- 劫持系统调用仅限教学与实验环境，切勿在生产系统操作。
- 卸载模块前确保用户态程序已退出，避免悬挂引用或阻塞。
- 每次修改代码后重新 `make` 并检查 `dmesg`，确认无未处理的错误日志。

---
## 14. 简要命令备忘
```bash
# 查看 sys_call_table 地址
sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms

# 实时查看内核日志
dmesg -w

# 手动创建设备节点（若 udev 失败）
sudo mknod /dev/m2mchardev c <major> 0
sudo chmod 666 /dev/m2mchardev
```

---
## 15. 结语
Lab3 将“模块生命周期、字符设备 I/O、系统调用跳转表”三个主题串联起来，涵盖了从简单日志到内核同步、再到危险操作的完整梯度。复习时可先从 hello 模块快速回顾 API，再深入字符设备的同步逻辑，最后讲解系统调用表的劫持与风险，对内核编程的关键概念做到心中有数。
