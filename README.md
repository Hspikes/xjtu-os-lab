# Operating Systems Lab Portfolio

[简体中文](README.zh-CN.md)

This repository contains the code and result screenshots from three rounds of operating systems labs. The exercises cover user-space processes and threads, inter-process communication, page-replacement simulation, kernel modules, and system-call table modification. Most of the code is written in C, and Lab 3 requires a Linux kernel development environment.

## Directory Overview

- `1lab/`: introductory process and thread exercises using `fork`, `exec`, `wait`, signals, mutexes, semaphores, and a simple spinlock. Runtime screenshots are under `res/`.
- `2lab/`: exercises in inter-process communication and memory management, including `kill`, pipes and locks, `alarm`, concurrent memory allocation, and FIFO/LRU page-replacement simulators. Runtime screenshots are under `res/`.
- `3lab/`: kernel modules, a character device, and system-call table modification. See the [Lab 3 guide](3lab/README.md) for details.

## Requirements

- A Linux development environment with `gcc`, `make`, and the headers required by `pthread` and POSIX semaphores.
- Lab 3 requires headers matching the running kernel and root privileges for loading and unloading modules.

## Quick Start

The user-space exercises can be compiled directly with `gcc`:

```bash
# Process and thread examples
gcc 1lab/1/1-1.c -o 1lab/1/1-1
gcc 1lab/2/2-2.c -o 1lab/2/2-2 -pthread
./1lab/2/2-2

# FIFO/LRU page-replacement simulator
gcc 2lab/4/4.c -o 2lab/4/4
./2lab/4/4
```

Use the Makefiles in the corresponding subdirectories for the kernel exercises:

```bash
cd 3lab/hello_test     && make && sudo insmod mymodules.ko && sudo rmmod mymodules
cd 3lab/dev            && make && sudo insmod glo_pro.ko   && sudo rmmod glo_pro
cd 3lab/modify_syscall && make && sudo insmod modify_syscall.ko p_sys_call_table=<addr> && sudo rmmod modify_syscall
```

> `p_sys_call_table` must be the actual address for the running kernel. It can be found through `/proc/kallsyms`. See the [Lab 3 guide](3lab/README.md) for details.

## Lab Summary

- **Lab 1:** process creation and waiting, parent-child signaling, and thread synchronization with mutexes, semaphores, and a spinlock.
- **Lab 2:** synchronization and races involving signals and pipes, `alarm` timers, process/thread safety, dynamic memory allocation, and implementations of FIFO and LRU page replacement.
- **Lab 3:** loadable kernel modules, a character-device message queue, and system-call table replacement and inspection, including CR0 write protection, wait queues, and `poll`/`epoll` support.

## Further Reading

- Source comments and the screenshots under each `res/` directory show how the exercises behave at runtime.
- Complete steps, review questions, and debugging notes for Lab 3 are in the [Lab 3 guide](3lab/README.md).
