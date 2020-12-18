// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/fs.h>      /* for register_chrdev */
#include <linux/kernel.h>  /* We're doing kernel work */
#include <linux/module.h>  /* Specifically, a module */
#include <linux/string.h>  /* for memset. NOTE - not string.h!*/
#include <linux/uaccess.h> /* for get_user and put_user */

MODULE_LICENSE("GPL");

// Our custom definitions of IOCTL operations
#include "message_slot.h"

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release(struct inode *inode, struct file *file)
{
    printk("Invoking device_release(%p,%p)\n", inode, file);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    return SUCCESS;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
    int i;
    char the_message[100] = "Hello World!";
    printk("Invoking device_write(%p,%ld)\n", file, length);
    for (i = 0; i < length && i < BUF_LEN; ++i)
    {
        get_user(the_message[i], &buffer[i]);
    }

    // return the number of input characters used
    return i;
}

//----------------------------------------------------------------
static long device_ioctl(struct file *file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    // Switch according to the ioctl called
    if (MSG_SLOT_CHANNEL == ioctl_command_id)
    {
        // Get the parameter given to ioctl by the process
        printk("Invoking ioctl: setting encryption "
               "flag to %ld\n",
               ioctl_param);
    }

    return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc = -1;
    // Register driver capabilities. Obtain major num
    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops);

    // Negative values signify an error
    if (rc < 0)
    {
        printk(KERN_ALERT "%s registraion failed for  %d\n", DEVICE_RANGE_NAME, MAJOR_NUM);
        return rc;
    }

    printk("Registeration is successful. ");
    printk("If you want to talk to the device driver,\n");
    printk("you have to create a device file:\n");
    printk("mknod /dev/%s c %d 0\n", DEVICE_RANGE_NAME "_file1", MAJOR_NUM);
    printk("You can echo/cat to/from the device file.\n");
    printk("Dont forget to rm the device file and "
           "rmmod when you're done\n");

    return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
