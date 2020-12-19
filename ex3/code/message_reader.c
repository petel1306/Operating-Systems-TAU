#include "message_slot.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Reads a message via message slot.
 *
 * argv[1]: message slot file path
 * argv[2]: the target message channel id. Assume a non-negative integer
 */

#define eprintf(...) fprintf(stderr, ##__VA_ARGS__)

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        eprintf("Invalid arguments amount: %d\n", argc);
        return 1;
    }

    char *path = argv[1];
    unsigned int channel_id = (unsigned int)atoi(argv[2]);
    char message[BUF_LEN] = {'\0'};

    int slot = open(path, O_RDONLY);
    if (slot < 0)
    {
        eprintf("open failed");
        return 1;
    }

    if (ioctl(slot, MSG_SLOT_CHANNEL, channel_id) < 0)
    {
        perror("ioctl failed");
        return 1;
    }

    printf("%d\n", slot);
    int message_size = read(slot, message, BUF_LEN);
    if (message_size == -1)
    {
        perror("read failed");
        return 1;
    }

    close(slot);

    if (write(STDOUT_FILENO, message, message_size) == -1)
    {
        perror("write to standart output failed");
        return 1;
    }

    return 0;
}