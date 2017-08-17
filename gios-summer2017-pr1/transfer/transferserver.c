#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#if 0
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 4000

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: cs8803.txt)\n" \
    "  -h                  Show this help message\n"         \
    "  -p                  Port (Default: 8140)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 8140;             /* port to listen on */
    char *filename = "cs8803.txt"; /* file to transfer */

    int socketfd, sockaddr_size, new_socketfd, filefd, option = 1;
    ssize_t sent_size;
    struct sockaddr_in server_addr, client_addr;
    struct stat filestat;

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        case 'f': // listen-port
            filename = optarg;
            break;
        }
    }

    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    // Create a socket
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        perror("Could not create a socket\n");
        exit(1);
    }
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    // Setup for bind
    memset((char *)&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portno);

    // Bind
    if( bind(socketfd,(struct sockaddr *)&server_addr , sizeof(server_addr)) < 0)
    {
        perror("Bind failed\n");
        exit(1);
    }

    // Listen
    if (listen(socketfd , 8) < 0){
        perror("Listen failed\n");
        exit(1);
    }

    //Accept incoming connection
    sockaddr_size = sizeof(struct sockaddr_in);
    while( (new_socketfd = accept(socketfd, (struct sockaddr *)&client_addr, (socklen_t*)&sockaddr_size)) ){
        // Open file and send file
        filefd = open(filename, O_RDONLY, S_IRUSR);
        if (filefd < 0) {
            perror("Could not open the file");
            exit(1);
        }

        fstat(filefd, &filestat);

        sent_size = sendfile(new_socketfd, filefd, 0, filestat.st_size);

        if (sent_size == -1) {
            perror("Error in sendfile");
            exit(1);
        }
        if (sent_size != filestat.st_size) {
            perror("Incomplete transfer from sendfile");
            exit(1);
        }

        //Clean up
        if (close(filefd) < 0) {
            perror("Could not close file");
            exit(1);
        }
        if (close(new_socketfd) < 0) {
            perror("Could not close socket\n");
            exit(1);
        }
    }

    if (new_socketfd<0)
    {
        perror("Connection accept failed\n");
        exit(1);
    }

    return 0;
}
