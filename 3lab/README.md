# Lab 3 Guide: Kernel Modules, Character Devices, and the System-Call Table

[简体中文](README.zh-CN.md)

This guide covers the code in each Lab 3 subdirectory, build and execution steps, the kernel concepts involved, common problems, and useful points for review or demonstration.

---

## Contents

1. Overview and Objectives
2. Code Structure
3. Environment and Preparation
4. Hello Module (`hello_test`)
5. Character-Device Message Queue (`dev`)
   - Data Structures and Synchronization
   - Read and Write Paths
   - `poll`/`epoll` Support
   - User-Space Test Programs
6. System-Call Hooking Experiment (`modify_syscall`)
   - Mechanism and Safety
   - Key Code
   - User-Space Verification
7. `sys_call_table` Inspection Module (`check`)
8. Build, Load, and Unload Commands
9. Common Problems and Debugging
10. Review Questions
11. Glossary

---

## 1. Overview and Objectives

Lab 3 focuses on three areas of Linux kernel programming:

- **Kernel module fundamentals:** understand module loading, unloading, and kernel log output.
- **Character-device drivers:** implement an in-kernel message queue with blocking and nonblocking I/O and `poll` support.
- **System-call table modification:** explore `sys_call_table` hooking, the CR0 write-protect bit, and restoration of the original entry.

Together, the exercises cover:

- kernel APIs such as `module_init`, `module_exit`, `cdev`, wait queues, and `copy_to_user`/`copy_from_user`;
- synchronization and concurrency using mutexes, wait queues, and atomic variables;
- kernel addresses and safety considerations involving `sys_call_table` and the CR0 WP bit.

---

## 2. Code Structure

- `hello_test/`: a minimal loadable kernel module.
- `dev/`: the `/dev/m2mchardev` character device and user-space read/write programs.
- `modify_syscall/`: a system-call hooking module and two verification programs.
- `check/`: a procfs module that inspects the current entry at `sys_call_table[96]`.

---

## 3. Environment and Preparation

1. Use Linux kernel headers that match the running kernel; building on the same test machine is usually the simplest approach.
2. Root privileges are required to load and unload modules with `sudo insmod` and `sudo rmmod`.
3. Obtain the address of `sys_call_table` with a command such as `sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms`. The address varies across kernel versions and boots.
4. Run the system-call experiment only in a virtual machine or disposable test environment. Modifying the system-call table can crash the kernel.

---

## 4. Hello Module (`hello_test`)

File: `hello_test/mymodules.c`

- **Core behavior:** `my_module_init` and `my_module_exit` write a log message when the module is loaded and unloaded.
- **Key points:**
  - `module_init` and `module_exit` register the entry and exit functions.
  - `MODULE_LICENSE("GPL")` declares the module license, avoids the corresponding kernel taint warning, and permits access to GPL-only symbols.
  - Messages written with `printk` can be viewed with `dmesg | tail`.
- **Purpose:** verify the kernel build environment and understand the module lifecycle.

Example:

```bash
cd hello_test
make
sudo insmod mymodules.ko
dmesg | tail
sudo rmmod mymodules
dmesg | tail
```

---

## 5. Character-Device Message Queue (`dev`)

Core file: `dev/glo_pro.c`

User-space tests: `dev/read.c`, `dev/write.c`

### 5.1 Design Goals

The `/dev/m2mchardev` character device implements an in-kernel message queue that allows multiple processes or threads to:

- write messages subject to total-capacity and per-message limits;
- read messages in FIFO order;
- use blocking or nonblocking I/O and `poll`/`epoll`.

### 5.2 Main Constants

- `M2M_DEVICE_NAME = "m2mchardev"`: device name.
- `MAX_MSG = 4 * 1024`: maximum message size of 4 KiB.
- `MAX_BYTES = 64 * 1024`: maximum total queued data of 64 KiB.

### 5.3 Internal Data Structures

- `struct msg`: a linked-list node containing a message length and data pointer.
- `struct mydev`: global device state containing:
  - `struct cdev devm`: the character-device object;
  - `struct list_head msg_list`: the message list;
  - `size_t queued_bytes`: the current total number of queued bytes;
  - `struct mutex lock`: protection for the queue and byte count;
  - `wait_queue_head_t readq/writeq`: wait queues for blocked readers and writers;
  - `atomic_t readers/writers`: counts of open read and write endpoints.
- `struct client_ctx`: per-open-file context recording read/write roles; this implementation allows both by default.

### 5.4 Main Functions and Control Flow

- `msg_alloc`/`msg_free`: allocate and release message storage with `kmalloc` and `kfree`.
- `m2m_open`:
  - allocates a `client_ctx` with read and write access;
  - increments the reader and writer counts.
- `m2m_release`: frees the client context and decrements the counts.
- `m2m_read`:
  - returns `-EAGAIN` if the queue is empty and the file is nonblocking;
  - otherwise waits with `wait_event_interruptible(readq, !list_empty(...))`;
  - removes the first message, copies it with `copy_to_user`, frees it, updates `queued_bytes`, and wakes waiting writers.
- `m2m_write`:
  - returns `-EMSGSIZE` when a message exceeds `MAX_MSG`;
  - when the write would exceed `MAX_BYTES`, returns `-EAGAIN` in nonblocking mode or waits with `wait_event_interruptible` in blocking mode;
  - copies data with `copy_from_user`, appends the message, updates `queued_bytes`, and wakes waiting readers.
- `m2m_poll`:
  - registers both wait queues with `poll_wait`;
  - reports `POLLIN | POLLRDNORM` when the queue is nonempty and `POLLOUT | POLLWRNORM` when space is available.

### 5.5 Device Registration and Cleanup

Initialization uses `alloc_chrdev_region` or `register_chrdev_region`, followed by `cdev_add`, `class_create`, and `device_create`. Unloading reverses those operations and frees any messages still in the queue.

### 5.6 User-Space Test Programs

- `write.c` opens `/dev/m2mchardev` with `O_RDWR`, reads lines from standard input, and writes them to the device. Multiple writers can be started to exercise queueing and blocking.
- `read.c` repeatedly reads up to 1024 bytes from the device and prints each message. It exits after receiving `"quit"`.

### 5.7 Build and Run

```bash
cd dev
make
sudo insmod glo_pro.ko
./write &   # writer
./read      # reader
# Enter several lines in the writer; the reader prints them in message order.
sudo rmmod glo_pro
```

### 5.8 Discussion Points

- Why use a mutex rather than a spinlock? The I/O paths can sleep while waiting or copying to and from user space.
- How do blocking and nonblocking modes differ? The implementation checks `O_NONBLOCK` and either returns `-EAGAIN` or sleeps on a wait queue.
- Under exactly what conditions should `poll`/`epoll` report the device as readable or writable?

---

## 6. System-Call Hooking Experiment (`modify_syscall`)

Core file: `modify_syscall/modify_syscall.c`

User-space tests: `modify_old_syscall.c`, `modify_new_syscall.c`

### 6.1 Approach

1. Find the kernel address of `sys_call_table` at runtime or through `System.map`.
2. Temporarily clear the CR0 `WP_BIT` to permit writes to kernel read-only memory.
3. Replace `sys_call_table[96]`, used by the lab as the original `gettimeofday` entry, with a custom `hello(a, b)` function.
4. Restore the original entry when unloading the module.

### 6.2 Key Code

- `clear_wp`/`restore_wp`: inline assembly that reads and updates CR0 to clear and restore write protection.
- `p_sys_call_table`: supplied as a module parameter; the target entry is calculated as `p_sys_call_table + SYS_NO * sizeof(void *)`.
- `hello(a, b)`: the replacement call, which returns the sum and logs the invocation with `printk`.
- `modify_syscall`: saves the old function pointer and installs the replacement.
- `restore_syscall`: writes back the original pointer during module unload.

### 6.3 Safety Notes

- An incorrect address can immediately panic the kernel. The address must match the running kernel.
- Disabling write protection is dangerous and belongs only in an isolated lab environment.
- Pass `p_sys_call_table` as a module parameter rather than relying on an address from a different kernel or boot.

### 6.4 User-Space Verification

- `modify_old_syscall.c`: before replacement, calls `syscall(96, &tv, NULL)` to obtain the time.
- `modify_new_syscall.c`: after replacement, calls `syscall(96, 10, 20)` and expects the result `30`.

### 6.5 Example

```bash
cd modify_syscall
make
# Find the address with: sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms
sudo insmod modify_syscall.ko p_sys_call_table=0xffffffffXXXXXXXX
./modify_new_syscall    # prints a + b
sudo rmmod modify_syscall
```

---

## 7. `sys_call_table` Inspection Module (`check`)

File: `check/check_syscall_entry.c`

- The `p_sys_call_table` module parameter supplies the table address.
- The module creates `/proc/syscall96`, which displays:
  - the supplied table address;
  - the current function address stored in `sys_call_table[96]`.
- It can be loaded before and after the hooking module to confirm that the entry changed and was restored.

Example:

```bash
cd check
make
sudo insmod check_syscall_entry.ko p_sys_call_table=0xffffffffXXXXXXXX
cat /proc/syscall96
sudo rmmod check_syscall_entry
```

---

## 8. Build, Load, and Unload Commands

### 8.1 Build

Run `make` in the corresponding subdirectory.

### 8.2 Load and Unload

- Hello module:

  ```bash
  sudo insmod hello_test/mymodules.ko
  sudo rmmod mymodules
  ```

- Character device:

  ```bash
  sudo insmod dev/glo_pro.ko
  sudo rmmod glo_pro
  ```

- System-call hooking module:

  ```bash
  sudo insmod modify_syscall/modify_syscall.ko p_sys_call_table=0xffffffffXXXXXXXX
  sudo rmmod modify_syscall
  ```

- Inspection module:

  ```bash
  sudo insmod check/check_syscall_entry.ko p_sys_call_table=0xffffffffXXXXXXXX
  sudo rmmod check_syscall_entry
  ```

---

## 9. Common Problems and Debugging

1. **`insmod` reports an unknown symbol or a kernel-version mismatch**
   - Confirm that the kernel headers match the running kernel.
2. **The device node does not exist**
   - Check whether `device_create` succeeded. If necessary, create the node with `mknod /dev/m2mchardev c <major> 0`.
3. **A blocked read or write never returns**
   - Check whether the file was opened with `O_NONBLOCK`, whether another process is producing or consuming messages, and whether `queued_bytes` has reached the limit.
4. **The system crashes after hooking the call**
   - Reboot the test machine, verify the `p_sys_call_table` address, and ensure the original entry is restored before unloading.
5. **The procfs file has no output or cannot be read**
   - Confirm that the module loaded successfully, the address parameter is correct, and `/proc/syscall96` is read with sufficient privileges.
6. **The kernel log is too noisy**
   - Use `dmesg | tail -n 50` for recent messages or `dmesg -w` to follow new messages.

---

## 10. Review Questions

### 10.1 Module Fundamentals

- **What do `module_init` and `module_exit` do?** They register the functions called when the module is loaded and unloaded; those functions acquire and release its resources.
- **Why does `MODULE_LICENSE` matter?** It declares the license, affects kernel tainting, and determines whether GPL-only symbols are available.

### 10.2 Character Devices

- **Why are `copy_to_user` and `copy_from_user` required?** User-space pointers cannot be safely dereferenced directly by the kernel; these APIs perform checked transfers across the boundary.
- **How are blocking and nonblocking modes implemented?** The driver checks `O_NONBLOCK`; nonblocking calls return `-EAGAIN`, while blocking calls sleep with `wait_event_interruptible`.
- **How does `poll` determine readability and writability?** A nonempty queue produces `POLLIN`; available queue capacity produces `POLLOUT`.
- **Why use a mutex instead of a spinlock?** The read and write paths may sleep in wait queues or during user-memory access, so they require a sleeping lock.

### 10.3 System-Call Table

- **Where is `sys_call_table`?** Its version-dependent address may be obtained from `/proc/kallsyms` or the matching `System.map` when the environment permits access.
- **Why clear the CR0 WP bit?** The table resides in read-only kernel memory; the experiment temporarily disables write protection to modify it.
- **How is the change undone?** The unload path restores the original function pointer and re-enables write protection.
- **What are the risks?** A bad address, an incorrect function signature, or concurrent calls can crash the kernel. Direct table modification must not be used in production.

---

## 11. Glossary

- **`cdev`:** the central character-device structure bound to `file_operations`.
- **`file_operations`:** callbacks implementing operations such as `open`, `read`, `write`, and `poll`.
- **`wait_event_interruptible`:** sleeps until a condition becomes true and permits interruption by a signal.
- **`copy_to_user`/`copy_from_user`:** checked data transfer between kernel-space and user-space buffers.
- **`sys_call_table`:** a table mapping system-call numbers to function pointers.
- **`CR0.WP`:** the write-protect bit that prevents writes to read-only kernel pages.
- **`poll`/`epoll`:** I/O multiplexing interfaces for observing whether file descriptors are ready.

---

## 12. Further Exploration

- Add a procfs or `seq_file` statistics interface showing the current queue length and cumulative write count.
- Add access control for dedicated readers and writers through open flags or `ioctl`.
- Replace direct `sys_call_table` modification with safer instrumentation such as ftrace, kprobes, or eBPF.

---

## 13. Rules and Reminders

- Hook system calls only in an isolated teaching or lab environment, never on a production system.
- Stop user-space test programs before unloading a module to avoid blocked operations or dangling references.
- Rebuild after every code change and inspect `dmesg` for unhandled errors.

---

## 14. Command Reference

```bash
# Show the sys_call_table address
sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms

# Follow the kernel log
dmesg -w

# Create the device node manually if udev did not create it
sudo mknod /dev/m2mchardev c <major> 0
sudo chmod 666 /dev/m2mchardev
```

---

## 15. Conclusion

Lab 3 connects module lifecycle management, character-device I/O, and system-call dispatch in a progression from a minimal logging module through kernel synchronization to a deliberately dangerous table modification. A useful review path is to start with the hello module, move on to the message queue and its synchronization, and finish with the system-call experiment and its safety implications.
