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

#if 0
/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
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

#define BUFSIZE 2000

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8140)\n"                                \
"  -m                  Maximum pending connections (default: 8)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"port",          required_argument,      NULL,           'p'},
        {"maxnpending",   required_argument,      NULL,           'm'},
        {"help",          no_argument,            NULL,           'h'},
        {NULL,            0,                      NULL,             0}
};


int main(int argc, char **argv) {
    int option_char;
    int portno = 8140; /* port to listen on */
    int maxnpending = 8;

    int socketfd, sockaddr_size, new_socketfd, option = 1;
    struct sockaddr_in server_addr, client_addr;
    char message[BUFSIZE], client_reply[BUFSIZE];

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:m:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
            case 'p': // listen-port
                portno = atoi(optarg);
                break;
            case 'm': // server
                maxnpending = atoi(optarg);
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
        }
    }

    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
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
    if (listen(socketfd , maxnpending) < 0){
        perror("Listen failed\n");
        exit(1);
    }

    //Accept incoming connection
    sockaddr_size = sizeof(struct sockaddr_in);
    while( (new_socketfd = accept(socketfd, (struct sockaddr *)&client_addr, (socklen_t*)&sockaddr_size)) ){
        //Receive a reply from the server
        memset(client_reply, '\0', BUFSIZE);
        if( read(new_socketfd, client_reply , BUFSIZE) < 0)
        {
            printf("Message receive from client failed\n");
            exit(1);
        }
        perror(client_reply);
        fflush(stdout);
        // Reply to the client
        strcpy(message, client_reply);
        write(new_socketfd, message, strlen(message)+1);

        if (close(new_socketfd) < 0) {
            printf("Could not close socket\n");
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
