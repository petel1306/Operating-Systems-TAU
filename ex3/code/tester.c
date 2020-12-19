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

/* tries to read an empty channel */
void read_no_message(int fd) {
	printf("\n----- read_no_message ---------- \n");
	fflush(stdout);
	int passed=1;
	char bffr[10];
	int rc = ioctl(fd, MSG_SLOT_CHANNEL, 20);
	if (rc == -1) {
        fprintf(stderr, "read_no_message: ioctl failed with error: %d\n", errno);
		return;
	}
	rc = read(fd, bffr, 5);
	if (rc == -1) {
	    if(errno != EWOULDBLOCK) {
            passed=0;
            fprintf(stderr, "read_no_message: read failed with wrong type of error: %d\n", errno);
            fprintf(stderr, "read_no_message: errno should be EWOULDBLOCK (11)\n");
        }
	}
	else{
        passed=0;
        fprintf(stderr, "read_no_message: read hasn't failed although he should have\n");
    }
	if(passed){
        fprintf(stderr,"read_no_message: PASSED!\n");
	}
	else{
        fprintf(stderr,"read_no_message: FAILED!\n");
    }
}

/* tries to read / write to / from a NULL pointer */
void write_read_null(int fd) {
	printf("\n----- write_read_null ---------- \n");
	fflush(stdout);
	int passed=1;
	sleep(1);
	char *null_bffr = NULL;
	int rc = ioctl(fd, MSG_SLOT_CHANNEL, 10);
	if (rc == -1) {
	    passed=0;
        fprintf(stderr, "write_read_null: ioctl failed with error: %d\n", errno);
        return;
	}
	rc = write(fd, null_bffr, 100);
	if (rc != -1) {
        passed=0;
        fprintf(stderr, "write_read_null: write hasn't failed although he should have\n");
    }
	rc = read(fd, null_bffr, 100);
	if (rc != -1) {
        passed=0;
        fprintf(stderr, "write_read_null: read hasn't failed although he should have\n");
	}
    if(passed){
        fprintf(stderr,"write_read_null: PASSED!\n");
    }
    else{
        fprintf(stderr,"write_read_null: FAILED!\n");
    }
}

/* tries to read / write a message too long */
void error_buffer_size(int fd) {
	printf("\n----- error_buffer_size ---------- \n");
	fflush(stdout);
	int passed=1;
	char *long_bffr = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	char* bffr = "a message bigger than short_buff size";
	char short_bffr[10];
	int rc = write(fd, long_bffr, 200);
	if (rc == -1) {
        if(errno!=EMSGSIZE) {
            passed=0;
            fprintf(stderr, "error_buffer_size: write failed with wrong type of error: %d\n", errno);
            fprintf(stderr, "error_buffer_size: errno should be EMSGSIZE (122)\n");
        }
	}
	else{
        passed=0;
        fprintf(stderr, "error_buffer_size: write hasn't failed although he should have\n");
	}
	rc = write(fd, bffr, 37);
	if (rc == -1) {
        passed=0;
        fprintf(stderr, "error_buffer_size: write failed with error: %d\n", errno);
        return;	}
	rc = read(fd, short_bffr, 10);
	if (rc == -1) {
        if(errno!=ENOSPC) {
            passed=0;
            fprintf(stderr, "error_buffer_size: read failed with wrong type of error: %d\n", errno);
            fprintf(stderr, "error_buffer_size: errno should be ENOSPC (28)\n");
        }
	}
	else{
        passed=0;
        fprintf(stderr, "error_buffer_size: read hasn't failed although he should have\n");
	}
    if(passed){
        fprintf(stderr,"error_buffer_size: PASSED!\n");
    }
    else{
        fprintf(stderr,"error_buffer_size: FAILED!\n");
    }
}

/* tries to read / write before envoking ioctl() command */
void write_read_before_ioctl(int fd) {
	printf("\n----- write_read_before_ioctl ---------- \n");
	fflush(stdout);
    int passed=1;
	char *bffr = "abc";
	char bffr_read[10];
	int rc = write(fd, bffr, 3);
	if (rc == -1) {
        if(errno!=EINVAL) {
            passed=0;
            fprintf(stderr, "write_read_before_ioctl: write failed with wrong type of error: %d\n", errno);
            fprintf(stderr, "write_read_before_ioctl: errno should be EINVAL (22)\n");
        }
	}
    else{
        passed=0;
        fprintf(stderr, "write_read_before_ioctl: write hasn't failed although he should have\n");
    }
	rc = read(fd, bffr_read, 3);
	if (rc == -1) {
	    if(errno!=EINVAL) {
            passed=0;
            fprintf(stderr, "write_read_before_ioctl: read failed with wrong type of error: %d\n", errno);
            fprintf(stderr, "write_read_before_ioctl: errno should be EINVAL (22)\n");
	    }
    }
    else{
        passed=0;
        fprintf(stderr, "write_read_before_ioctl: read hasn't failed although he should have\n");
    }
	rc = ioctl(fd, MSG_SLOT_CHANNEL, 10);
	if (rc == -1) {
        fprintf(stderr, "write_read_before_ioctl: ioctl failed with error: %d\n", errno);
        return;
	}
	rc = write(fd, bffr, 3);
	if (rc == -1) {
        passed=0;
        fprintf(stderr, "write_read_before_ioctl: write failed with error: %d\n", errno);
    }
	rc = read(fd, bffr_read, 3);
	if (rc == -1) {
        passed=0;
        fprintf(stderr, "write_read_before_ioctl: read failed with error: %d\n", errno);
	} else {
		bffr_read[rc] = '\0'; /* terminating the string */
	}
    if(strcmp(bffr,bffr_read)!=0){
        passed=0;
        fprintf(stderr, "write_read_before_ioctl: read value doesnt match written message\n");
    }
    if(passed){
        fprintf(stderr,"write_read_before_ioctl: PASSED!\n");
    }
    else{
        fprintf(stderr,"write_read_before_ioctl: FAILED!\n");
    }
}

int main(int argc, char *argv[]) {
	int fd = open(argv[1], O_RDWR); /* argv[1] is a device created beforehand */
    srand(time(NULL));
    if (fd < 0) {
	    fprintf(stderr, "Can't open device file: %s\n", argv[1]);
	    return -1;
	}

    write_read_before_ioctl(fd);
	error_buffer_size(fd);
	read_no_message(fd);
	write_read_null(fd);
	close(fd);
	return 0;
}
