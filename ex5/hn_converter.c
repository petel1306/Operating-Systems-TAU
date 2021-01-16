#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        return 1;
    }
    char *const command = argv[1];
    uint32_t number;
    sscanf(argv[2], "%u", &number);

    if (strcmp(command, "h2n") == 0)
    {
        int net_number = htonl(number);
        char *net_buf = (char *)&net_number;
        printf("Convert host value = %u , to network value = %u\n"
               "type: echo -n -e '\\x%x\\x%x\\x%x\\x%x | nc 127.0.0.1 5555\n",
               number, htonl(number), net_buf[0], net_buf[1], net_buf[2], net_buf[3]);
        return 0;
    }
    else if (strcmp(command, "n2h") == 0)
    {
        printf("Convert network value = %u , to host value = %u\n", number, ntohl(number));
        return 0;
    }
    else
    {
        return 1;
    }
}