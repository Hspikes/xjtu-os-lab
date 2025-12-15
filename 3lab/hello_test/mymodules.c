#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>

// #if CONFIG_MODVERSION==1
// #define MODVERSION
// #include<linux/modversions.h>
// #endif

static int my_module_init(void)
{
    printk("Hello! This is a testing module!\n");
    return 0;
}

static void my_module_exit(void)
{
    printk("Goodbye! The testing module is unloading now!\n");
}

module_init(my_module_init);
module_exit(my_module_exit);
MODULE_LICENSE("GPL");