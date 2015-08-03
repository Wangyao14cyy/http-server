#include <linux/module.h>
#include <linux/init.h>

MODULE_LICENSE("Dual BSD/GPL");
static int hello_init(void)
{
    printk(KERN_INFO"hello world!");
    return 0;
}
static void hello_exit(void)
{
    printk(KERN_INFO"GOODBEY BURDEN!");
}
module_init(hello_init);
module_exit(hello_exit);
