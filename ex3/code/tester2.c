#include "message_slot.h" /* replace it with your own header if needed */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h> /* open */
#include <sys/ioctl.h>  /* ioctl */
#include <unistd.h> /* read, write */
#include <errno.h>
#include <string.h>
#include <time.h>
#include "math.h"

#define BUFF_SIZE 128
int DEBUG = 0; /*change to 1 to see PASSING tests*/
int status=1;
char* channels_msg[1024];

/* limit to 1024 channels for easy test */
int get_random_channel(){
    int max_channel = pow(2,10);
    return (rand() % max_channel);
};

char *get_rand_string(char *str, size_t size)
{
    size_t n;
    int key;
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    if (size) {
        --size;
        for (n = 0; n < size; n++) {
            key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}


int perform_random_write(int fd) {
    int rc, channel,length,passed;
    char *bffr;
    passed=1;
    length = rand()%(2*BUFF_SIZE);
    bffr=(char*)malloc(length);
    get_rand_string(bffr,(size_t)length);
    channel = get_random_channel();
    rc = ioctl(fd, MSG_SLOT_CHANNEL, channel);
    if (rc == -1) {
        if(channel>0) {
            fprintf(stderr, "perform_random_write: ioctl failed with error: %d\n", errno);
            fprintf(stderr, "DETAILS:\n"
                            "fd is - %d,\n"
                            "channel ID is - %d.\n", fd, channel);
        }
        if(DEBUG){fprintf(stderr,"perform_random_write: PASSED!\n");}
        return 0;
    }
    rc = write(fd, bffr, length);
    if(length>0 && length <=BUFF_SIZE) {
        if (rc == -1) {
            passed = 0;
            fprintf(stderr, "perform_random_write: write failed with error: %d\n", errno);
            fprintf(stderr, "DETAILS:\n"
                            "fd is - %d,\n"
                            "bffr to write is - %s,\n"
                            "bffr length is - %d.\n", fd, bffr, length);
        }
    }
    else{
        if (rc != -1) {
            passed=0;
            fprintf(stderr, "perform_random_write: write hasn't failed although he should have\n");
            fprintf(stderr, "DETAILS:\n"
                            "fd is - %d,\n"
                            "bffr length is - %d.\n", fd, length);
        }
    }

    if(passed){
        if (DEBUG){fprintf(stderr,"perform_random_write: PASSED!\n");}
        if(channel>0 && length>0 && length <=BUFF_SIZE) {
            channels_msg[channel] = bffr;
        }

    }
    else{
        fprintf(stderr,"perform_random_write: FAILED!\n");
        status=0;
        return -1;
    }
    return 0;
}



int perform_random_read(int fd){
    int rc, channel,passed;
    char bffr[BUFF_SIZE];
    passed=1;
    channel = get_random_channel();
    rc = ioctl(fd, MSG_SLOT_CHANNEL, channel);
    if (rc == -1) {
        if(channel>0) {
            fprintf(stderr, "perform_random_read: ioctl failed with error: %d\n", errno);
            fprintf(stderr, "DETAILS:\n"
                            "fd is - %d,\n"
                            "channel ID is - %d.\n", fd, channel);
        }
        if (DEBUG){fprintf(stderr,"perform_random_read: PASSED!\n");}
        return 0;
    }
    rc = read(fd, bffr, BUFF_SIZE);
    if (rc == -1) {
        if(channels_msg[channel]==0){
            if(errno != EWOULDBLOCK) {
                passed=0;
                fprintf(stderr, "perform_random_read: read failed with wrong type of error: %d\n", errno);
                fprintf(stderr, "perform_random_read: errno should be EWOULDBLOCK (11)\n");
            }
        }
        else {
            passed = 0;
            fprintf(stderr, "perform_random_read: read failed with error: %d\n", errno);
            fprintf(stderr, "DETAILS:\n"
                            "fd is - %d,\n"
                            "channel ID is - %d,\n"
                            "channel_msg is - %s\n", fd, channel,channels_msg[channel]);
        }
    }

    if(-1!=rc && strcmp(bffr,channels_msg[channel])!=0){
        passed=0;
        fprintf(stderr, "perform_random_read: read value doesnt match written message\n");
        fprintf(stderr, "DETAILS:\n"
                        "fd is - %d,\n"
                        "channel ID is - %d,\n"
                        "msg written is - %s\n"
                        "msg read is - %s.\n", fd,channel,channels_msg[channel],bffr);
    }
    if(passed){
        if (DEBUG){fprintf(stderr,"perform_random_read: PASSED!\n");}
    }
    else{
        fprintf(stderr,"perform_random_read: FAILED!\n");
        status=0;
        return -1;
    }
    return 0;
}

void free_all(){
    int i;
    for (i=0;i<1024;i++){
        if(channels_msg[i]!=0){
            free(channels_msg[i]);
        }
    }
}
int perform_random_move(int fd){
    int rc=0,move = rand() % 2;

    switch (move) {
        case 0:
            rc=perform_random_read(fd);
            break;
        case 1:
            rc=perform_random_write(fd);
            break;
    }
    return rc;

}

/* does a 2^18 random write or read actions */
void random_write_read(int fd) {
    printf("\n----- random_write_read ---------- \n");
    memset(&channels_msg,0, sizeof(channels_msg));
    for (int i = 0; i < pow(2, 18); i++) {
        if(DEBUG){fprintf(stderr,"iteration number %d .",i);}
        if(-1==perform_random_move(fd)){
            return;
        }
    }
    free_all();
}



int main(int argc, char *argv[]) {
    int fd = open(argv[1], O_RDWR); /* argv[1] is a device created beforehand */
    srand(time(NULL));
    if (fd < 0) {
        fprintf(stderr, "Can't open device file: %s\n", argv[1]);
        return -1;
    }
    random_write_read(fd);
    if(status){
        fprintf(stderr, "random_write_read: PASSED!\n");
    }
    else{
        fprintf(stderr, "random_write_read: FAILED!\n");
    }

    close(fd);
    return 0;
}
