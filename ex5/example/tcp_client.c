#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

// MINIMAL ERROR HANDLING FOR EASE OF READING

int main(int argc, char *argv[])
{
  int  sockfd     = -1;
  int  bytes_read =  0;
  char recv_buff[1024];

  struct sockaddr_in serv_addr; // where we Want to get to
  struct sockaddr_in my_addr;   // where we actually connected through 
  struct sockaddr_in peer_addr; // where we actually connected to
  socklen_t addrsize = sizeof(struct sockaddr_in );

  memset(recv_buff, 0,sizeof(recv_buff));
  if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("\n Error : Could not create socket \n");
    return 1;
  }

  // print socket details
  getsockname(sockfd,
              (struct sockaddr*) &my_addr,
              &addrsize);
  printf("Client: socket created %s:%d\n",
         inet_ntoa((my_addr.sin_addr)),
         ntohs(my_addr.sin_port));

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(10000); // Note: htons for endiannes
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // hardcoded...

  printf("Client: connecting...\n");
  // Note: what about the client port number?
  // connect socket to the target address
  if( connect(sockfd,
              (struct sockaddr*) &serv_addr,
              sizeof(serv_addr)) < 0)
  {
    printf("\n Error : Connect Failed. %s \n", strerror(errno));
    return 1;
  }

  // print socket details again
  getsockname(sockfd, (struct sockaddr*) &my_addr,   &addrsize);
  getpeername(sockfd, (struct sockaddr*) &peer_addr, &addrsize);
  printf("Client: Connected. \n"
         "\t\tSource IP: %s Source Port: %d\n"
         "\t\tTarget IP: %s Target Port: %d\n",
         inet_ntoa((my_addr.sin_addr)),    ntohs(my_addr.sin_port),
         inet_ntoa((peer_addr.sin_addr)),  ntohs(peer_addr.sin_port));

  // read data from server into recv_buff
  // block until there's something to read
  // print data to screen every time
  while( 1 )
  {
    bytes_read = read(sockfd,
                      recv_buff,
                      sizeof(recv_buff) - 1);
    if( bytes_read <= 0 )
      break;
    recv_buff[bytes_read] = '\0';
    puts( recv_buff );
  }

  close(sockfd); // is socket really done here?
  //printf("Write after close returns %d\n", write(sockfd, recv_buff, 1));
  return 0;
}
