#ifndef HTTP_H
#define HTTP_H

/*
 * Read an HTTP request from an active TCP connection socket
 * fd: The socket's file descriptor
 * resource_name: Set to the name of the requested resource on success
 * Returns 0 on success or -1 on error
 */
int read_http_request(int fd, char *resource_name);

/*
 * Write an HTTP response to an active TCP connection socket
 * fd: The socket's file descriptor
 * resource_path: The path to the requested resource in the server's file system
 * Returns 0 on success or -1 on error
 */
int write_http_response(int fd, const char *resource_path);

#endif // HTTP_H
