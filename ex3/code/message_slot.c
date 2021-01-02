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

// #define INFO(message, ...) printk("\n\nMessage_Slot-%d: " message "\n\n", get_info_counter(), ##__VA_ARGS__)
#define INFO(...)
// #define SHOW(var, type) INFO(#var " = %" #type, var)
#define SHOW(...)
// -================ Define database structures =================

// Using Linux kernel lists
typedef struct
{
    unsigned int minor;
    unsigned int id;
    unsigned int message_size;
    char message[BUF_LEN];

    struct list_head list_node;

} channel_t;

static struct list_head slots[SLOT_AMOUNT];

/**
 * Finds the channel with a specific minor and id
 * Returns NULL if not found
 */
channel_t *find_channel(unsigned int minor, unsigned int id)
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
 * Adds a channel with a specific minor and id
 * Returns channel if succeed, NULL if allocation failed
 */
channel_t *add_channel(unsigned int minor, unsigned int id)
{
    struct list_head *slot;
    channel_t *channel;

    slot = &slots[minor];

    // Allocate a new channel
    channel = (channel_t *)kmalloc(sizeof(channel_t), GFP_KERNEL);

    // If allocation failed
    if (channel == NULL)
    {
        return NULL;
    }

    // Add the channel to the slot
    channel->minor = minor;
    channel->id = id;
    list_add_tail(&channel->list_node, slot);
    return channel;
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
        slot = &slots[i];

        list_for_each_entry_safe(the_channel, temp_channel, slot, list_node)
        {

            // Remove the channel from the list
            list_del(&the_channel->list_node);

            // Free the memory
            kfree(the_channel);
        }
    }
}

// ================= Define file handlers =======================
static int const UINT = sizeof(unsigned int);

channel_t *get_file_channel(const struct file *_file)
{
    return _file->private_data;
}

/**
 * Set the channel id to the file
 */
int set_file_channel(struct file *_file, unsigned int minor, unsigned int channel_id)
{
    // channel_t *previous_channel = _file->private_data;
    // unsigned int minor = previous_channel->minor;

    channel_t *new_channel = find_channel(minor, channel_id);
    if (new_channel == NULL)
    {
        new_channel = add_channel(minor, channel_id);
        if (new_channel == NULL)
        {
            return -ENOMEM;
        }
    }
    _file->private_data = new_channel;
    return SUCCESS;
}

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode, struct file *_file)
{
    int debug_ret;
    channel_t *debug_channel;

    // Get the minor of the file
    unsigned int minor = iminor(inode);

    INFO("Invoking device_open(%p)\n", _file);

    // Associate the defualt channel 0 to the file
    debug_ret = set_file_channel(_file, minor, DEFAULT_CHANNEL_ID);
    debug_channel = get_file_channel(_file);
    SHOW(debug_channel, p);
    return debug_ret;
}

//----------------------------------------------------------------
static long device_ioctl(struct file *_file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    int debug_ret;
    channel_t *debug_channel;

    // Switch according to the ioctl called
    if (MSG_SLOT_CHANNEL == ioctl_command_id)
    {
        unsigned int minor = get_file_channel(_file)->minor;
        unsigned int channel_id = (unsigned int)ioctl_param;

        INFO("Invoking device_ioctl(%p)\n", _file);

        if (channel_id == 0)
        {
            return -EINVAL;
        }

        debug_ret = set_file_channel(_file, minor, channel_id);
        debug_channel = get_file_channel(_file);
        SHOW(debug_channel, p);
        return debug_ret;
    }
    return -EINVAL;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file *_file, char __user *buffer, size_t length, loff_t *offset)
{
    channel_t *channel;
    char message_debug[BUF_LEN + 1] = {'\0'};

    INFO("Invoking device_read(%p,%ld)\n", _file, length);

    // Get the required channel
    channel = get_file_channel(_file);

    // In case no channel has been set
    if (channel->id == DEFAULT_CHANNEL_ID)
    {
        return -EINVAL;
    }

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

    memcpy(message_debug, channel->message, BUF_LEN);
    INFO("message_length: %d, message: %s", channel->message_size, message_debug);
    return channel->message_size;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file *_file, const char __user *buffer, size_t length, loff_t *offset)
{
    channel_t *channel;
    char message_debug[BUF_LEN + 1] = {'\0'};

    INFO("Invoking device_write(%p,%ld)\n", _file, length);

    // Get the required channel
    channel = get_file_channel(_file);

    // In case no channel has been set
    if (channel->id == DEFAULT_CHANNEL_ID)
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

    memcpy(message_debug, channel->message, BUF_LEN);
    INFO("message_length: %d, message: %s", channel->message_size, message_debug);
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
    int i = 0;

    if (register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops) < 0)
    {
        INFO(KERN_ERR "%s registraion failed for  %d\n", DEVICE_RANGE_NAME, MAJOR_NUM);
        return -1;
    }

    // Initialize the slots' linked list head
    for (i = 0; i < SLOT_AMOUNT; i++)
    {
        INIT_LIST_HEAD(&slots[i]);
    }

    INFO("Registeration is successful!\n\n\n\n\n\n\n\n\n");

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
