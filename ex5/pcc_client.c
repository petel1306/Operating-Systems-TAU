#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const size_t SIZE_BOUND = sizeof(u_int32_t);
socklen_t addrsize = sizeof(struct sockaddr_in);

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Invalid arguments amount\n");
        return 1;
    }

    // ============== Setting the server address ===================

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);       // Set the IP
    serv_addr.sin_port = htons((u_int16_t)atoi(argv[2])); // Set the port

    // ============== Mapping the input file to the virtual memory ================

    int filefd = open(argv[3], O_RDONLY);
    if (filefd == -1)
    {
        perror("Can not open the designated file");
        return 1;
    }

    struct stat sb;
    if (fstat(filefd, &sb) == -1)
    {
        perror("Couldn't get the file size");
        return 1;
    }

    uint32_t file_size = (uint32_t)sb.st_size;
    // printf("file size is %u\n", file_size); // Debug

    char *const file_buf = (char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, filefd, 0);
    if (file_buf == MAP_FAILED)
    {
        perror("Memory allocation failed");
        return 1;
    }

    // Debug
    // for (int i = 0; i < file_size; i++)
    // {
    //     printf("%c", file_buf[i]);
    // }
    // printf("\n");

    // ============= Connecting to the server ====================

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Could not create socket");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, addrsize) == -1)
    {
        perror("Connect failed");
        return 1;
    }

    // Debug
    // struct sockaddr_in my_addr;   // where we actually connected through
    // struct sockaddr_in peer_addr; // where we actually connected to
    // getsockname(sockfd, (struct sockaddr *)&my_addr, &addrsize);
    // getpeername(sockfd, (struct sockaddr *)&peer_addr, &addrsize);
    // printf("Client: Connected. \n\t\tSource IP: %s Source Port: %d\n\t\tTarget IP: %s Target Port: %d\n",
    //        inet_ntoa((my_addr.sin_addr)), ntohs(my_addr.sin_port), inet_ntoa((peer_addr.sin_addr)),
    //        ntohs(peer_addr.sin_port));

    // =============== Communicating with the server ==================
    int nsent;

    // Sending the size of the file
    u_int32_t file_size_net = htonl(file_size);
    nsent = write(sockfd, &file_size_net, SIZE_BOUND);
    if (nsent != SIZE_BOUND)
    {
        perror("Send failed");
        return 1;
    }

    // Transferring the content of the file
    char *buf_index = file_buf;
    uint32_t bytes_left = file_size;
    while (bytes_left > 0)
    {
        nsent = write(sockfd, buf_index, bytes_left);
        if (nsent <= 0)
        {
            perror("Send failed");
            return 1;
        }
        buf_index += nsent;
        bytes_left -= nsent;
    }

    // Receiving the number of printable characters
    uint32_t printable_chars_net;
    nsent = read(sockfd, &printable_chars_net, SIZE_BOUND);
    if (nsent != SIZE_BOUND)
    {
        perror("Receive failed");
        return 1;
    }
    uint32_t printable_chars = ntohl(printable_chars_net);

    // =================== Epilogue =====================
    munmap(file_buf, file_size);
    close(sockfd);
    close(filefd);

    printf("# of printable characters: %u\n", printable_chars);
    return 0;
}