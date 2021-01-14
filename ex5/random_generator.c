#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define RAND_PATH "/dev/urandom"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Invalid arguments amount\n");
        return 1;
    }

    unsigned int size = atoi(argv[1]);
    char *const path = argv[2];

    char *buf = calloc(size, sizeof(char));

    FILE *rand_file = fopen(RAND_PATH, "r");
    fread(buf, sizeof(char), size, rand_file);
    fclose(rand_file);

    FILE *out_file = fopen(path, "w");
    fwrite(buf, sizeof(char), size, out_file);
    fclose(out_file);

    free(buf);
    return 0;
}