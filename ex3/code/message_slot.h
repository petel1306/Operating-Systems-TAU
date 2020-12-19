#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

// The major device number.
#define MAJOR_NUM 240

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SLOT_AMOUNT 256

#define SUCCESS 0

#endif