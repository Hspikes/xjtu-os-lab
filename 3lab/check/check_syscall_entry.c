// check_syscall_entry.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define SYS_NO 96

static unsigned long p_sys_call_table = 0;
module_param(p_sys_call_table, ulong, 0444);
MODULE_PARM_DESC(p_sys_call_table, "Address of sys_call_table");

static int show_syscall_entry(struct seq_file *m, void *v)
{
    unsigned long *sys_call_addr;
    unsigned long entry = 0;

    seq_printf(m, "p_sys_call_table = 0x%lx\n", p_sys_call_table);
    if (!p_sys_call_table) {
        seq_printf(m, "sys_call_table not provided\n");
        return 0;
    }

    sys_call_addr = (unsigned long *)(p_sys_call_table + SYS_NO * sizeof(void *));
    entry = READ_ONCE(*sys_call_addr);
    seq_printf(m, "sys_call_table[%d] @ %p = 0x%lx\n", SYS_NO, sys_call_addr, entry);
    return 0;
}

static int proc_open_syscall(struct inode *inode, struct file *file)
{
    return single_open(file, show_syscall_entry, NULL);
}

static const struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .open = proc_open_syscall,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init chk_init(void)
{
    if (!proc_create("syscall96", 0444, NULL, &proc_fops)) {
        pr_err("failed to create /proc/syscall96\n");
        return -ENOMEM;
    }
    pr_info("check_syscall_entry loaded. Use: cat /proc/syscall96\n");
    pr_info("p_sys_call_table param currently = 0x%lx\n", p_sys_call_table);
    return 0;
}

static void __exit chk_exit(void)
{
    remove_proc_entry("syscall96", NULL);
    pr_info("check_syscall_entry unloaded\n");
}

module_init(chk_init);
module_exit(chk_exit);
MODULE_LICENSE("GPL");
