#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512
#define MAXMIME 30

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

// NOTE: This server only handles GET requests
int read_http_request(int fd, char *resource_name) {
    // stdio FILE * gives us 'fgets()' to easily read line by line
    int fd_copy = dup(fd);
    if (fd_copy == -1) {
        perror("dup");
        return -1;
    }
    FILE *socket_stream = fdopen(fd_copy, "r");
    if (socket_stream == NULL) {
        perror("fdopen");
        close(fd_copy);
        return -1;
    }
    // Disable the usual stdio buffering
    if (setvbuf(socket_stream, NULL, _IONBF, 0) != 0) {
        perror("setvbuf");
        fclose(socket_stream);
        return -1;
    }

    // Keep consuming lines until we find an empty line
    // This marks the end of the response headers and beginning of actual content
    char buf[BUFSIZE];
    char resource_buf[BUFSIZE];
    int first_line = 1;
    while (fgets(buf, BUFSIZE, socket_stream) != NULL) {
        if (first_line == 1) {
            // Copy the first buffer's contents
            // Don't lose resource name upon long request
            if (strcpy(resource_buf, buf) == NULL) {
                fclose(socket_stream);
                return -1;
            }
        }
        first_line = 0;
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
    }

    char *delimeter = " ";
    char *token;
    // Extract resource name
    token = strtok(resource_buf, delimeter);
    if (!token) {
        printf("strtokerror");
        fclose(socket_stream);
        return -1;
    }
    // Check that request is a valid GET request
    if (strcmp(token, "GET") != 0) {
        fclose(socket_stream);
        return -1;
    }
    // token should now hold the resource name
    token = strtok(NULL, delimeter);
    if (!token) {
        fclose(socket_stream);
        return -1;
    }
    
    // Copy resource name into resource_name
    if (strcpy(resource_name, token) == NULL) {
        fclose(socket_stream);
        return -1;
    }
    // Close copied file descriptor
    if (fclose(socket_stream) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    struct stat file_stat;
    int outcome;
    if (stat(resource_path, &file_stat) == -1) {
        if (errno != ENOENT) {
            // Unrelated error
            perror("stat");
            return -1;
        }
        outcome = 404;
    } else {
        outcome = 200;
    }
    char header[BUFSIZE];
    if (outcome == 200) {
        const char *content_type = get_mime_type(strrchr(resource_path, '.'));
        if (!content_type) {
            content_type = "application/octet-stream";
        }
        int file_fd = open(resource_path, O_RDONLY);
        if (file_fd == -1) {
            // Opening file failed, return 404 Not Found instead of crashing
            outcome = 404;
        }
        // Make sure file properly opened
        if (outcome == 200) {
            // Fill in 200 OK response into header
            if (snprintf(header, BUFSIZE, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", content_type, file_stat.st_size) < 0) {
                perror("snprintf");
                return -1;
            }
            // Write response to client
            if (write(fd, header, strlen(header)) == -1) {
                if (errno != ECONNRESET) {
                    perror("write");
                    close(file_fd);
                    return -1;
                }
                fprintf(stderr, "write_http_response: Connection reset by peer\n");
                if (close(file_fd) == -1) {
                    perror("close");
                    return -1;
                }
                return 0;
            }
            // Start copying data from file to client
            char buf[BUFSIZE];
            int read_len;
            while ((read_len = read(file_fd, buf, BUFSIZE)) > 0) {
                if (write(fd, buf, read_len) < 0) {
                    if (errno != ECONNRESET) {
                        perror("write");
                        close(file_fd);
                        return -1;
                    }
                    // If client disconnects early
                    fprintf(stderr, "write_http_response: Connection reset by peer\n");
                    close(file_fd);
                    return 0;
                }
            }
            if (close(file_fd) == -1) {
                perror("close");
                return -1;
            }
        }
    }
    if (outcome == 404) {
        // Fill in 404 Not Found response into header
        if (snprintf(header, BUFSIZE, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n") < 0) {
            printf("snprintf");
            return -1;
        }
        // Write response to client
        if (write(fd, header, strlen(header)) == -1) {
            if (errno != ECONNRESET) {
                perror("write");
                return -1;
            }
            fprintf(stderr, "write_http_response: Connection reset by peer\n");
        }

    }
    
    return 0;
}
