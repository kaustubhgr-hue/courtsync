/*
 * scoreboard.c - Linux kernel module that creates /dev/scoreboard.
 *
 * The device holds ONE text buffer in kernel memory: the latest scoreboard
 * state line (JSON) written by the C engine.
 *   write  -> replaces the stored state with what was written
 *   read   -> returns the stored state (cat /dev/scoreboard)
 * A mutex protects the buffer so a reader never sees a half-written state.
 */
#include <linux/module.h>      /* module_init, MODULE_LICENSE, ... */
#include <linux/miscdevice.h>  /* misc_register: easy way to get a /dev node */
#include <linux/fs.h>          /* struct file_operations */
#include <linux/mutex.h>       /* DEFINE_MUTEX, mutex_lock */
#include <linux/slab.h>        /* kmalloc, kfree */
#include <linux/uaccess.h>     /* copy_from_user, simple_read_from_buffer */

#define STATE_MAX 4096         /* engine JSON is well under this */

/* Shared data. "static" keeps these private to this module. */
static char state_buf[STATE_MAX];
static size_t state_len;                /* bytes currently stored (0 = nothing yet) */
static DEFINE_MUTEX(state_lock);        /* only one reader/writer touches state_buf at a time */

/*
 * Called when a program reads the device (e.g. cat).
 * *ppos is the read position for this open file. simple_read_from_buffer
 * copies from our buffer to user space starting at *ppos and advances it,
 * so the second read returns 0 (end of file) and cat stops.
 */
static ssize_t sb_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    mutex_lock(&state_lock);
    ret = simple_read_from_buffer(ubuf, count, ppos, state_buf, state_len);
    mutex_unlock(&state_lock);
    return ret;
}

/*
 * Called when a program writes to the device (e.g. echo ... > /dev/scoreboard).
 * Each write replaces the whole stored state.
 * User-space memory can't be used directly in the kernel, so we copy it in
 * with copy_from_user. We copy into a temporary buffer first, so a failed copy
 * never leaves the stored state half-overwritten.
 */
static ssize_t sb_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    char *tmp;

    if (count == 0)
        return 0;
    if (count > STATE_MAX)
        return -EINVAL;                 /* too big: reject, keep old state */

    tmp = kmalloc(count, GFP_KERNEL);   /* GFP_KERNEL: normal allocation, may sleep */
    if (!tmp)
        return -ENOMEM;
    if (copy_from_user(tmp, ubuf, count)) {
        kfree(tmp);
        return -EFAULT;                 /* bad user pointer */
    }

    mutex_lock(&state_lock);
    memcpy(state_buf, tmp, count);
    state_len = count;
    mutex_unlock(&state_lock);

    kfree(tmp);
    return count;                       /* tell the caller all bytes were accepted */
}

/* Table of functions the kernel calls for operations on our device. */
static const struct file_operations sb_fops = {
    .owner = THIS_MODULE,
    .read  = sb_read,
    .write = sb_write,
};

/*
 * A "misc" device: the kernel picks a device number and udev creates
 * /dev/scoreboard automatically. mode 0666 lets normal users read/write,
 * so the engine doesn't need sudo.
 */
static struct miscdevice sb_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "scoreboard",
    .fops  = &sb_fops,
    .mode  = 0666,
};

/* Runs on insmod. */
static int __init sb_init(void)
{
    int ret = misc_register(&sb_dev);

    if (ret) {
        pr_err("scoreboard: misc_register failed (%d)\n", ret);
        return ret;
    }
    pr_info("scoreboard: loaded, /dev/scoreboard ready\n");   /* shows in dmesg */
    return 0;
}

/* Runs on rmmod. */
static void __exit sb_exit(void)
{
    misc_deregister(&sb_dev);
    pr_info("scoreboard: unloaded\n");
}

module_init(sb_init);
module_exit(sb_exit);

MODULE_LICENSE("GPL");          /* required, or the kernel marks itself "tainted" and limits APIs */
MODULE_AUTHOR("CourtSync");
MODULE_DESCRIPTION("Holds the latest CourtSync scoreboard state in /dev/scoreboard");
