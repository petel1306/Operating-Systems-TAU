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

#define CHAR_AMOUNT (1 << 8)
uint32_t pcc_total[CHAR_AMOUNT];
int listenfd;
volatile sig_atomic_t is_interrupted = 0; // Indicates if the program got SIGINT

const size_t SIZE_BOUND = sizeof(u_int32_t);
socklen_t addrsize = sizeof(struct sockaddr_in);

extern int errno;

int is_client_problem()
{
    return (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE);
}

void epilogue(void)
{
    close(listenfd);

    // Print the statistics
    for (char c = 32; c <= 126; c++)
    {
        printf("char '%c' : %u times\n", c, pcc_total[c]);
    }

    exit(0);
}

void interrupt_handler(int num)
{
    is_interrupted = 1;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Invalid arguments amount\n");
        return 1;
    }

    // ================= Registering signal handler =====================
    struct sigaction sa;
    sa.sa_handler = interrupt_handler;
    sa.sa_flags = SA_RESTART; // making system calls restartable across signals
    sigaction(SIGINT, &sa, NULL);

    // ============== Setting the listening server ===================

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);        // Set the IP
    serv_addr.sin_port = htons((u_int16_t)atoi(argv[1])); // Set the port

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("Could not create socket");
        return 1;
    }

    int enable = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
    {
        perror("Failed to set socket options");
        return 1;
    }

    if (bind(listenfd, (struct sockaddr *)&serv_addr, addrsize) == -1)
    {
        perror("Connect failed");
        return 1;
    }

    if (listen(listenfd, 10) == -1)
    {
        perror("Listen failed");
        return 1;
    }

    // ========================= Handle incoming connections =======================

    struct sockaddr_in peer_addr;
    while (1)
    {
        if (is_interrupted)
        {
            epilogue();
        }

        // Turning off system calls restartable for a valid accept functionality
        sa.sa_flags = sa.sa_flags & (~SA_RESTART);
        sigaction(SIGINT, &sa, NULL);

        int connfd = accept(listenfd, (struct sockaddr *)&peer_addr, &addrsize);
        if (connfd < 0)
        {
            if (errno == EINTR)
            {
                epilogue();
            }

            perror("Accept Failed");
            return 1;
        }

        // Turning on system calls restartable for upcoming system calls
        sa.sa_flags = sa.sa_flags | SA_RESTART;
        sigaction(SIGINT, &sa, NULL);

        int nsent;

        // Receiving the size of the file
        u_int32_t file_size_net;
        nsent = read(connfd, &file_size_net, SIZE_BOUND);
        if (nsent != SIZE_BOUND)
        {
            perror("Receive failed");
            if (is_client_problem())
            {
                goto finish_loop2;
            }
            else
            {
                return 1;
            }
        }
        const u_int32_t file_size = ntohl(file_size_net);

        // Allocating buffer to hold the file
        char *const file_buf =
            (char *)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (file_buf == MAP_FAILED)
        {
            perror("Memory allocation failed");
            return 1;
        }

        // Receiving the content of the file
        char *buf_index = file_buf;
        uint32_t bytes_left = file_size;
        while (bytes_left > 0)
        {
            nsent = read(connfd, buf_index, bytes_left);
            if (nsent == 0)
            {
                fprintf(stderr, "Client terminated unexpectedly\n");
                goto finish_loop1;
            }

            if (nsent == -1)
            {
                perror("Receive failed");
                if (is_client_problem())
                {
                    goto finish_loop1;
                }
                else
                {
                    return 1;
                }
            }

            buf_index += nsent;
            bytes_left -= nsent;
        }

        // Processing the content of the file
        uint32_t printable_chars = 0;
        char c;
        for (int i = 0; i < file_size; i++)
        {
            c = file_buf[i];

            // Check if printable
            if (32 <= c && c <= 126)
            {
                printable_chars++;
            }

            // Add to pcc total
            pcc_total[c]++;
        }

        // Sending the number of printable characters
        uint32_t printable_chars_net = htonl(printable_chars);
        nsent = write(connfd, &printable_chars_net, SIZE_BOUND);

    finish_loop1:                    // ***After allocation was done
        munmap(file_buf, file_size); // Free the addresses
    finish_loop2:                    // ***After connection was created
        close(connfd);               // Close the connection
    }
}