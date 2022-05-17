/* http_server.c -- a basic HTTP server application
 * Based on Beej's server.c
 * Supports simultaneous HTTP GET requests, responding with
 * status 200 OK, 404 Not Found, or 400 Bad Request.
 * Author: Eric Roch
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10	 		// how many pending connections queue will hold

// function signatures
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int sock_getline(int sock, char *buf, int size);
int main(int argc, char *argv[]);
int bind_server(const char *port);
void handle_client(int client);
int process_request(char *request, char *method, char *URI, char *version);
void respond_bad_request(int client);
void respond_not_found(int client);
void respond_ok(int client, const char *filename);
void send_file(int client, const char *filename);


void sigchld_handler(int s) {
	(void)s;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
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

int main(int argc, char *argv[]) {
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "usage: http_server <port>\n");
        exit(1);
    }

	sockfd = bind_server(argv[1]);

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("\nserver: got connection from %s\n", s);

		if (!fork()) {  	// this is the child process
			close(sockfd); 	// child doesn't need the listener
			handle_client(new_fd);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

/**
 * Bind a new socket to localhost at the specified port.
 * @param  port Port to bind socket to
 * @return      New socket file descriptor
 */
int bind_server(const char *port) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("http_server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("http_server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "http_server: failed to bind\n");
		return 2;
	}

	printf("http_server: socket bound to port %s\n", port);

	freeaddrinfo(servinfo); // all done with this structure

	return sockfd;
}

/**
 * Main handler function to process incoming requests (called by child processes).
 * Read the request from the client and respond with the requested file,
 * if it is available.  If the file is not found, respond with a 404 error.
 * If the request is malformed or the request method has not been implemented,
 * respond with a 400 error message.
 * @param client Client socket file descriptor
 */
void handle_client(int client) {
	// read incoming http request
	char request_line[1024];
	int n;
	n = sock_getline(client, request_line, sizeof(request_line));

	printf("request: %s\n", request_line);

	// consume the rest of the header, we don't need it
	char discard[256];
	discard[0] = 'A'; discard[1] = '\0';
	while((n > 0) && strcmp(discard, "\n")) {
		sock_getline(client, discard, sizeof(discard));
	}

	// parse request line from header
	char method[32];
	char uri[256];
	char version[32];

	int rv = process_request(request_line, method, uri, version);
	if (rv == -1) {
		respond_bad_request(client);
		return;
	}

	if (strcmp(method, "GET") != 0)	{		// This server only supports GET
		respond_bad_request(client);
		return;
	}

	printf("Method: %s\n", method);
	printf("URI: %s\n", uri);
	printf("Version: %s\n", version);

	char filename[256];			// files are served from the current working directory,
	strcpy(filename, uri+1);	// so strip off the leading '/' from the URI

	send_file(client, filename);
}

/**
 * Split Request into three parts: the request method, the URI, and HTTP
 * version number.  Each field is separated by a single space.  The buffers
 * passed in to hold the method, URI, and version must be large enough to
 * hold the split strings.  Note: although the state of request will be
 * modified, it will be restored before returning.
 * @param  request The HTTP header request-line to parse
 * @param  method  The HTTP method (GET, POST, etc.)
 * @param  URI     The resource URI for the request
 * @param  version The HTTP protocol version (HTTP/1.0, etc.)
 * @return         Return 0 on success, -1 if any of the string splits fail
 */
int process_request(char *request, char *method, char *URI, char *version) {
	int len = strlen(request);	// save a copy of request
	char temp[len];				// because the string splitting
	strcpy(temp, request);		// will overwrite the spaces with '\0'

	char *pch;

	pch = strtok(request, " ");	// split on the first space
	if (pch) {
		strcpy(method, pch);
	}
	else {
		strcpy(request, temp);
		return -1;
	}

	pch = strtok(NULL, " ");	// split on the second space
	if (pch) {
		strcpy(URI, pch);
	}
	else {
		strcpy(request, temp);
		return -1;
	}

	pch = strtok(NULL, "\n");	// split on the ending newline
	if (pch) {
		strcpy(version, pch);
	}
	else {
		strcpy(request, temp);
		return -1;
	}

	strcpy(request, temp);		// restore the original request string and return
	return 0;
}

/**
 * Send a basic 400 Bad Request error message.
 * @param client Socket to send on
 */
void respond_bad_request(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 400 Bad Request\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Bad Request</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the request contained\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "an error or that feature has not been implemented.</P>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**
 * Send a basic 404 Not Found error message.
 * @param client Socket to send on
 */
void respond_not_found(int client) {
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.</P>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**
 * Send a very basic 200 OK response.  The content type is
 * assumed to be text as clients should not process a text file.
 * Additional filetype checking could be performed to make this more accurate.
 * @param client Socket to send on
 */
void respond_ok(int client, const char *filename) {
	char buf[1024];
	(void)filename;		// can be used to guess a file type

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}

/**
 * Serve a file to the specified client.  Files are searched
 * for from the current working directory. If the file is not
 * found, respond with a 404 error.
 * @param client   Socket to send on
 * @param filename Filename of file to send
 */
void send_file(int client, const char *filename) {
	FILE *fptr, *sock;
	fptr = NULL;

	fptr = fopen(filename, "rb");
	if (fptr == NULL) {
		respond_not_found(client);
		return;
	}

	// send 200 OK header
	respond_ok(client, filename);

	sock = fdopen(client, "wb");
	char buffer[4096];
	int n;
	do {
		n = fread(buffer, 1, sizeof(buffer), fptr);
		if (n < sizeof(buffer)) {
			if (!feof(fptr)) {
				fprintf(stderr, "An error occured while reading from file %s\n", filename);
				break;
			}
		}
		fwrite(buffer, 1, n, sock);
	} while(!feof(fptr));

	fclose(sock);
	fclose(fptr);

	// Unbuffered read -> buffered send
	// // send file contents, filling a buffer before each send
	// char buf[1024];			// output buffer
	// int i = 0;				// buffer index
	// char c;					// current character
	//
	// c = fgetc(fptr);
	// while(!feof(fptr)) {
	// 	// fill the buffer (or hit the EOF)
	// 	while(i < sizeof(buf) && !feof(fptr)) {
	// 		buf[i++] = c;
	// 		c = fgetc(fptr);
	// 	}
	//
	// 	// `i` contains the next index to be written, aka the length
	// 	// of valid data.  Send `i` bytes from `buf` to `client`
	// 	send(client, &buf, i, 0);
	// 	i = 0;
	//
	// 	// Note: `c` already holds the next character from the
	// 	// buffer loop, so we can simply loop here and check EOF
	// 	// before filling the buffer again.
	// }
}
