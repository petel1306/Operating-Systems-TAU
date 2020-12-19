// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/fs.h>     /* for register_chrdev */
#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/module.h> /* Specifically, a module */
#include <linux/slab.h>
#include <linux/string.h>  /* for memset. NOTE - not string.h!*/
#include <linux/uaccess.h> /* for get_user and put_user */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ori Petel");

// Our custom definitions of IOCTL operations
#include "message_slot.h"

// ==================== Debug Functions ========================
static unsigned int info_counter = 0;

unsigned int get_info_counter(void)
{
    return ++info_counter;
}

#define INFO(message, ...) printk(KERN_DEBUG "\n\nMessage_Slot-%d: " message "\n\n", get_info_counter(), ##__VA_ARGS__);

// ================= Define file handlers =======================
static int const UINT = sizeof(unsigned int);

/**
 * Release the minor and the channel from the files
 */
void get_detais(const struct file *_file, unsigned int *minor, unsigned int *channel_id)
{
    memcpy(minor, _file->private_data, UINT);
    memcpy(channel_id, _file->private_data + UINT, UINT);
}

/**
 * Set the minor to the file
 */
void set_minor(struct file *_file, const unsigned int minor)
{
    memcpy(_file->private_data, &minor, UINT);
}

/**
 * Set the channel id to the file
 */
void set_channel_id(struct file *_file, const unsigned int channel_id)
{
    memcpy(_file->private_data + UINT, &channel_id, UINT);
}

// -================ Define database structures =================

// Using Linux kernel lists
typedef struct
{
    unsigned int id;
    unsigned int message_size;
    char message[BUF_LEN];

    struct list_head list_node;

} channel_t;

static struct list_head slots[SLOT_AMOUNT];

/**
 * Get the channel configured to the file
 */
channel_t *get_channel(unsigned int minor, unsigned int id)
{

    struct list_head *slot;
    channel_t *channel;

    slot = &slots[minor];

    // Find the required channel
    list_for_each_entry(channel, slot, list_node)
    {
        if (channel->id == id)
        {
            return channel;
        }
    }

    // In case not found
    return NULL;
}

/**
 * Add the channel configured to the file
 */
int add_channel(unsigned int minor, unsigned int id)
{
    struct list_head *slot;
    channel_t *channel;

    slot = &slots[minor];

    // Allocate a new channel
    channel = (channel_t *)kmalloc(sizeof(channel_t), GFP_KERNEL);

    // If allocation failed
    if (channel == NULL)
    {
        return -EHWPOISON;
    }

    // Add the channel to the slot
    channel->id = id;
    list_add_tail(&channel->list_node, slot);
    return SUCCESS;
}

/**
 * Free all the memory took by the slots
 */
void free_slots(void)
{
    int i;
    struct list_head *slot;
    channel_t *the_channel;
    channel_t *temp_channel;

    for (i = 0; i < SLOT_AMOUNT; i++)
    {
        slot = &slot[i];

        list_for_each_entry_safe(the_channel, temp_channel, slot, list_node)
        {

            // Remove the channel from the list
            list_del(&the_channel->list_node);

            // Free the memory
            kfree(the_channel);
        }
    }
}

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode, struct file *_file)
{
    printk("Invoking device_open(%p,%p)\n", inode, _file);

    set_minor(_file, iminor(inode));
    set_channel_id(_file, 0);

    return SUCCESS;
}

//----------------------------------------------------------------
static long device_ioctl(struct file *_file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    // Switch according to the ioctl called
    if (MSG_SLOT_CHANNEL == ioctl_command_id)
    {
        unsigned int f_minor, f_id;
        unsigned int id = (unsigned int)ioctl_param;

        // Get the parameter given to ioctl by the process
        printk("Invoking ioctl: setting encryption "
               "flag to %ld\n",
               ioctl_param);

        if (id == 0)
        {
            return -EINVAL;
        }
        set_channel_id(_file, id);

        // Get minor and channel id
        get_detais(_file, &f_minor, &f_id);

        // Check if the channel already exists
        if (get_channel(f_minor, f_id) == NULL)
        {
            return add_channel(f_minor, f_id);
        }

        return SUCCESS;
    }
    return -EINVAL;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file *_file, char __user *buffer, size_t length, loff_t *offset)
{
    unsigned int f_minor, f_id;
    channel_t *channel;

    printk("Invoking device_read(%p,%ld)\n", _file, length);

    // Get minor and channel id
    get_detais(_file, &f_minor, &f_id);

    if (f_id == 0)
    {
        return -EINVAL;
    }

    // Get the required channel
    channel = get_channel(f_minor, f_id);

    if (channel->message_size == 0)
    {
        return -EWOULDBLOCK;
    }

    if (length < channel->message_size)
    {
        return -ENOSPC;
    }

    if (copy_to_user(buffer, channel->message, channel->message_size))
    {
        return -EINVAL;
    }

    return channel->message_size;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file *_file, const char __user *buffer, size_t length, loff_t *offset)
{
    unsigned int f_minor, f_id;
    channel_t *channel;

    printk("Invoking device_write(%p,%ld)\n", _file, length);

    // Get minor and channel id
    get_detais(_file, &f_minor, &f_id);

    if (f_id == 0)
    {
        return -EINVAL;
    }

    if (length <= 0 || length > 128)
    {
        return -EMSGSIZE;
    }

    if (copy_from_user(channel->message, buffer, length))
    {
        return -EINVAL;
    }

    channel->message_size = length;
    return length;
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
        printk(KERN_ERR "%s registraion failed for  %d\n", DEVICE_RANGE_NAME, MAJOR_NUM);
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
