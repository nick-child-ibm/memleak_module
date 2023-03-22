#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick Child");
MODULE_DESCRIPTION("Module to test various memory leaks");

static void use_after_free(void)
{
    char *ptr = kmalloc(100, GFP_KERNEL);
    kfree(ptr);
    memset(ptr, 'c', 10);
}

static void double_free(void)
{
    char *ptr = kmalloc(100, GFP_KERNEL);
    kfree(ptr);
    kfree(ptr);
}

static void buffer_write_out_of_bounds(void)
{
    char *ptr = kmalloc(100, GFP_KERNEL);
    memset(ptr, 'f', 102);
    kfree(ptr);
}

static void buffer_unreferenced(void)
{
    char *ptr =  kmalloc(100, GFP_KERNEL);
    memset(ptr, 'f', 10);
}

struct test_case {
    char *name;
    void (*func)(void);
};
#define ADD_CASE(k) {.name =  __stringify(k), .func = k}
static struct test_case bad_funcs[] = {
                            ADD_CASE(use_after_free),
                            ADD_CASE(double_free),
                            ADD_CASE(buffer_write_out_of_bounds),
                            ADD_CASE(buffer_unreferenced)
                            };
static ssize_t cause_trouble_show(struct kobject *kobj,
                                  struct kobj_attribute *attr,
                                  char *buf)
{
    int at = 0;
    for (int i = 0; i < ARRAY_SIZE(bad_funcs); i++)
        at += sysfs_emit_at(buf, at, "%d - %s\n", i, bad_funcs[i].name);
    return at;
}
static ssize_t cause_trouble_store(struct kobject *k, struct kobj_attribute *a, const char *c, size_t n)
{
    int func;
    
    if (kstrtoint(c, 10, &func) || func >= ARRAY_SIZE(bad_funcs)
        || func < 0) {
        printk("%s is invalid\n", c);
        return -1;
    }
    printk("running %s\n", (char *)bad_funcs[func].name);
    bad_funcs[func].func();
    return n;
}

static struct kobject *k_obj;
static struct kobj_attribute cause_trouble = __ATTR(cause_trouble, 0660,
                                                    cause_trouble_show,
                                                    cause_trouble_store);

static int __init bad_init(void)
{
    printk(KERN_INFO "Loading bad driver!\n");
    k_obj = kobject_create_and_add("bad_mod", kernel_kobj);

    return sysfs_create_file(k_obj, &cause_trouble.attr);
}

static void __exit bad_exit(void)
{
    printk(KERN_INFO "Unloading bad driver\n");
    kobject_put(k_obj);
}
module_init(bad_init);
module_exit(bad_exit);
