/* http_client.c -- a simple HTTP client application
 * Based on Beej's client.c
 * Author: Eric Roch
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 1024 // max number of bytes we can get at once

// function signatures
void *get_in_addr(struct sockaddr *sa);
int sendall(int s, char *buf, int *len);
int sock_getline(int sock, char *buf, int size);
int main(int argc, char *argv[]);
void process_input(const char *input, char *host, char *port, char *path);
int connect_to_server(const char *hostname, const char *port);
void send_header(int sockfd, const char *host, const char *port, const char *path);
void process_response(int sockfd);

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int s, char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/**
 * Read a single line from socket, up to `size` bytes and place it
 * in `buf`.  If a newline is found before the buffer is filled, the
 * buffer is null terminated after adding the newline.  If no newline
 * is found, the buffer is null terminated after the first `size-1`
 * bytes read from the socket.
 * @param  sock Socket to read from
 * @param  buf  Buffer to store line in
 * @param  size Size of line buffer
 * @return      Number of bytes read in line
 */
int sock_getline(int sock, char *buf, int size) {
	int i = 0;
	char c = '\0';
	int n;

	// Read characters until we fill the buffer or find a newline
	while((i < size - 1) && (c != '\n')) {
		n = recv(sock, &c, 1, 0);
		if (n > 0) {
			// Each line in the buffer should end in \n, so if we find a
			// \r in the input, check if the next character is \n.  If yes,
			// consume the next char and add to the buffer, otherwise manually
			// add \n instead.
			if (c == '\r') {
				n = recv(sock, &c, 1, MSG_PEEK);
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else {
			c = '\n';
		}
	}
	// null terminate the buffer
	buf[i] = '\0';

	return i;
}

int main(int argc, char* argv[]) {
    int sockfd;              // file descriptor for socket

    char hostname[512];     // hostnames may be any string, lets assume they will be shorter than 512 characters
    char port[6];           // ports may be 0-65535
    char path[512];         // paths should be far shorter than 512 characters

    // zero out the buffers to avoid garbage data
    memset(hostname, 0, 512);
    memset(port, 0, 6);
    memset(path, 0, 512);

    if (argc != 2) {
        fprintf(stderr, "usage: http_client http://<host>:<port>/<path>\n");
        exit(1);
    }

    // get the hostname, port number, and file path
    process_input(argv[1], hostname, port, path);

    // create a new socket connection to the server
    if ((sockfd = connect_to_server(hostname, port)) == 0) {
        exit(2);
    }

    // create and send the HTTP header
    send_header(sockfd, hostname, port, path);

    // get the response and save the file contents
    process_response(sockfd);

    close(sockfd);

    return 0;
}

/**
 * Parse the input for the server hostname, port, and file path of the
 * requested download.  If no port is specified in the input, a default
 * value of 80 is chosen (standard HTTP port).  Similarly, if no file
 * path is given, a default value of "/index.html" is returned.
 * @param input string to be parsed
 * @param host  the hostname to connect to
 * @param port  the port to connect on
 * @param path  the path to download
 */
void process_input(const char *input, char *host, char *port, char *path) {
    char temp[strlen(input)];

    // strip "http://" from argument if necessary
    if (strncmp(input, "http://", 7) == 0)
        strcpy(temp, input+7);
    else
        strcpy(temp, input);

    // separate the hostname (and port) from the file path
    // if no '/' is found (path was not specified), assume "/index.html"
    char *pch;
    pch = strchr(temp, '/');
    if (pch != NULL) {
        int len = pch - temp;
        strncpy(host, temp, len);
        strcpy(path, pch);
    }
    else {
        strcpy(path, "/index.html");
    }

    // pull the port out of the hostname
    // if no port was specified, assume port 80
    pch = strchr(host, ':');
    if (pch != NULL) {
        int len = pch - host;
        strcpy(port, pch+1);
        host[len] = '\0';
    }
    else {
        strcpy(port, "80");
    }

	printf("Downloading %s from %s:%s\n", path, host, port);
}

/**
 * Create a connection to the specified host over the specified port and
 * return a socket file descriptor for the connection.
 * @param  hostname server to connect to
 * @param  port     port to connect on
 * @return          socket file descriptor, or zero if error
 */
int connect_to_server(const char *hostname, const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;   // structs for getting address info
    int rv;                                 // dummy variable for error checking
    char s[INET6_ADDRSTRLEN];               // buffer for server address string

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // set up the addrinfo struct for the server
    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 0;
    }

    // loop through servinfo results and connect to the first one we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("http_client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("http_client: connect");
            continue;
        }

        break;
    }

    // if we reached the end, print an error and quit
    if (p == NULL) {
        fprintf(stderr, "http_client: unable to connect to %s\n", hostname);
        return 0;
    }

    // print the connected host address
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr),
            s, sizeof(s));
    printf("http_client: connected to %s\n", s);

    // all done with this structure, so we can free it
    freeaddrinfo(servinfo);

    return sockfd;
}

/**
 * Send a simple HTTP/1.0 GET request over the provided socket.
 * @param sockfd socket to send over
 * @param host      hostname of connected server
 * @param port      port of connected server
 * @param path      path of requested file
 */
void send_header(int sockfd, const char *host, const char *port, const char *path) {
    char header[1024];
    printf("http_client: sending GET\n");

    sprintf(header, "GET %s HTTP/1.0\r\n", path);
    send(sockfd, header, strlen(header), 0);
    sprintf(header, "Host: %s:%s\r\n", host, port);
    send(sockfd, header, strlen(header), 0);
    strcpy(header, "Connection: close\r\n");
    send(sockfd, header, strlen(header), 0);
    strcpy(header, "\r\n");
    send(sockfd, header, strlen(header), 0);
}

/**
 * Save the response from the server as a file on the local machine. Do not
 * include the HTTP header of the response, only the actual file contents.
 * @param buffer the server's response
 */
void process_response(int sockfd) {
    printf("http_client: reading response...\n");

    char buffer[4096];   	// buffer for received data
	char status[32];		// buffer for status code

	// Extract the status code from the first line
	// and discard the rest of the header
	int n, i;

	n = sock_getline(sockfd, buffer, sizeof(buffer));
	for (i = 0; i < n; i++) {
		// copy everything after the first space, excluding the trailing \n
		if (buffer[i] == ' ') {
			strcpy(status, buffer+i+1);
			break;
		}
	}

	while((n = sock_getline(sockfd, buffer, sizeof(buffer))) != 0) {
		if (strcmp(buffer, "\n") == 0)
			break;
    }

	// if we got any status other than "200 OK" then something went wrong
	if (strncmp(status, "200 OK", 6) != 0) {
		fprintf(stderr, "Error, unable to download file: %s\n", status);
		return;
	}

	// read the socket as a file, echoing to the output
	// file `sizeof(buffer)` bytes at a time.
	FILE *out, *sock;
	out = fopen("output", "wb");
	sock = fdopen(sockfd, "rb");

	do {
		n = fread(buffer, 1, sizeof(buffer), sock);
		if (n < sizeof(buffer)) {
			if (!feof(sock)){
				fprintf(stderr, "An error occured while reading from the socket.\n");
				break;
			}
		}
		fwrite(buffer, 1, n, out);
	} while(!feof(sock));

    fclose(out);
}
