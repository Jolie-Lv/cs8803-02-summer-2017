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

#define BUFSIZE 2000

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferclient [options]\n"                           \
    "options:\n"                                             \
    "  -s                  Server (Default: localhost)\n"    \
    "  -p                  Port (Default: 8140)\n"           \
    "  -o                  Output file (Default gios.txt)\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"output", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8140;
    char *filename = "gios.txt";

    int socketfd, filefd;
    struct sockaddr_in server_addr;
    struct hostent* server_info;
    ssize_t read_size;
    char read_buffer[BUFSIZE];

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:h", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
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

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    // Create a socket
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        printf("Could not create a socket\n");
        exit(1);
    }

    // Setup for connect
    server_info = gethostbyname(hostname);
    if (server_info == NULL) {
        fprintf(stderr,"Cannot find host with name %s\n", hostname);
        exit(1);
    }
    memcpy(&server_addr.sin_addr.s_addr,server_info->h_addr,server_info->h_length);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portno);

    //Connect to server
    if (connect(socketfd , (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0)
    {
        printf("Error during connect\n");
        exit(1);
    }

    // Open a file
    filefd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (filefd < 0) {
        perror("Could not open the file");
        exit(1);
    }

    // Read socket and write to file until nothing is sent
    do {
        read_size = read(socketfd, read_buffer, BUFSIZE);
        if (read_size < 0) {
            perror("Read from socket error");
            exit(1);
        }
        if( write(filefd , read_buffer , read_size) < 0)
        {
            printf("Write to file failed\n");
            exit(1);
        }
    } while (read_size > 0);

    //Clean up
    if (close(filefd) < 0) {
        perror("Could not close file");
        exit(1);
    }
    if (close(socketfd) < 0) {
        perror("Could not close socket\n");
        exit(1);
    }

    return 0;
}
