#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>

#define SYS_NO 96
#define WP_BIT 0x00010000UL

unsigned long old_sys_call_func=0;

static unsigned long p_sys_call_table=0xffffffff964002c0;
// module_param(p_sys_call_table, ulong, 0444);
// MODULE_PARM_DESC(p_sys_call_table, "Address of sys_call_table (from System.map or /proc/kallsyms)");

static unsigned long clear_wp(void)
{
    unsigned long cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %0, %%cr0" :: "r"(cr0 & ~WP_BIT));
    return cr0;
}

static void restore_wp(unsigned long cr0)
{
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

int hello(int a,int b) //new function
{
    int r = a + b;
    printk("No 96 syscall has changed to hello");
    printk(KERN_INFO "hello called: a=%d b=%d -> r=%d\n", a, b, r);
    return a + b;
}

static int modify_syscall(void)
{
	unsigned long *sys_call_addr;
    unsigned long oldcr0;
    if(!p_sys_call_table)
    {
        pr_err("p_sys_call_table is 0 — pass address as module parameter\n");
        return -EINVAL;        
    }
    sys_call_addr = (unsigned long *)(p_sys_call_table + SYS_NO * sizeof(void *));
    old_sys_call_func = *(sys_call_addr);
    
    oldcr0 = clear_wp();
    *(sys_call_addr) = (unsigned long)&hello;
    restore_wp(oldcr0);

    pr_info("replaced syscall %d at %p, old=0x%lx\n", SYS_NO, sys_call_addr, old_sys_call_func);
    return 0;
}

static void restore_syscall(void)
{
    unsigned long *sys_call_addr;
    unsigned long oldcr0;
    if (!p_sys_call_table || !old_sys_call_func) return;

    sys_call_addr = (unsigned long *)(p_sys_call_table + SYS_NO * sizeof(void *));
    oldcr0 = clear_wp();
    *(sys_call_addr) = old_sys_call_func; // point to original function
    restore_wp(oldcr0);
    pr_info("restored syscall %d\n", SYS_NO);
}

static int mymodule_init(void)
{
    modify_syscall();
    return 0;
}

static void mymodule_exit(void)
{
    restore_syscall();
}

module_init(mymodule_init);
module_exit(mymodule_exit);
MODULE_LICENSE("GPL");

// sudo awk '/sys_call_table/ {print $1}' /proc/kallsyms
// p_sys_call_table=0xffffffff820002c0
// insmod modify_syscall.ko p_sys_call_table=0xffffffff964002c0
// insmod modify_syscall.ko p_sys_call_table=0xffffffff964013a0