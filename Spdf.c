#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define PORT 8081
#define BUF_SIZE 1024

void handle_client(int client_sock);
void handle_rmfile(const char *filename);
void handle_ufile(const char *filename, const char *dest_path, const char *file_content, int client_sock);
int make_dirs(const char *path);
void handle_dfile(const char *filename, int client_sock);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    pid_t child_pid;

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sock, 10) < 0) {
        perror("Listen error");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Spdf server listening on port %d\n", PORT);

    while (1) {
        // Accept a connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept error");
            continue;
        }

        // Fork a child process to handle the client
        if ((child_pid = fork()) == 0) {
            close(server_sock);
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        } else if (child_pid < 0) {
            perror("Fork error");
            close(client_sock);
        } else {
            close(client_sock);
            waitpid(-1, NULL, WNOHANG);
        }
    }

    close(server_sock);
    return 0;
}

void handle_client(int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;

    // Receive the command from the client
    n = recv(client_sock, buffer, BUF_SIZE, 0);
    if (n <= 0) {
        if (n == 0) {
            printf("Client disconnected before sending command\n");
        } else {
            perror("Recv error (command)");
        }
        close(client_sock);
        return;
    }
    buffer[n] = '\0';
    printf("Received command: %s\n", buffer);

    // Handle the "dfile" command
    if (strncmp(buffer, "dfile ", 6) == 0) {
        char *received_filename = buffer + 6;

        // Trim any newline characters
        received_filename[strcspn(received_filename, "\n")] = 0;

        // Handle the file download request
        handle_dfile(received_filename, client_sock);
    }

    // Check if the command is rmfile
    else if (strncmp(buffer, "rmfile ", 7) == 0) {
        char *received_filename = buffer + 7;
        handle_rmfile(received_filename);
    } else {
        // Parse ufile command
        char *received_filename = strtok(buffer, "\n");
        char *received_dest_path = strtok(NULL, "\n");
        char *received_file_content = strtok(NULL, "\0");  // Capture the remaining content as file content

        if (!received_filename || !received_dest_path) {
            printf("Invalid command format\n");
            close(client_sock);
            return;
        }

        handle_ufile(received_filename, received_dest_path, received_file_content, client_sock);
    }

    close(client_sock);
}

void handle_rmfile(const char *filename) {
    // Perform the file deletion
    if (remove(filename) == 0) {
        printf("File '%s' deleted successfully.\n", filename);
    } else {
        perror("File deletion error");
    }
}

void handle_ufile(const char *filename, const char *dest_path, const char *file_content, int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;
    FILE *file;

    // Create the directory if it doesn't exist
    if (make_dirs(dest_path) != 0) {
        perror("mkdir error");
        return;
    }

    char fullpath[BUF_SIZE];
    snprintf(fullpath, BUF_SIZE, "%s/%s", dest_path, filename);

    // Open file for writing
    if ((file = fopen(fullpath, "wb")) == NULL) {
        perror("File open error");
        return;
    }
    printf("Opened file for writing: %s\n", fullpath);

    // Write the initially received file content if any
    if (file_content) {
        size_t content_len = strlen(file_content);
        size_t written = fwrite(file_content, 1, content_len, file);
        if (written != content_len) {
            perror("fwrite error");
            fclose(file);
            return;
        }
        fflush(file);
    }

    // Receive and write additional file data
    while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
        printf("Writing %ld bytes to file\n", n);  // Debug statement
        printf("File content: %.*s\n", (int)n, buffer);  // Print file content
        size_t written = fwrite(buffer, 1, n, file);
        if (written != n) {
            perror("fwrite error");
            break;
        }
        fflush(file);  // Ensure data is written to the file immediately
    }

    if (n < 0) {
        perror("Recv error (file data)");
    } else {
        printf("File '%s' successfully stored in directory '%s'\n", filename, dest_path);
    }

    fclose(file);
}

int make_dirs(const char *path) {
    char tmp[BUF_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) && errno != EEXIST) {
        return -1;
    }
    return 0;
}

void handle_dfile(const char *filename, int client_sock) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("File open error");
        return;
    }

    char buffer[BUF_SIZE];
    ssize_t n;

    // Read and send the file content to the client
    while ((n = fread(buffer, 1, BUF_SIZE, file)) > 0) {
        if (send(client_sock, buffer, n, 0) == -1) {
            perror("Send error");
            break;
        }
        memset(buffer, 0, BUF_SIZE);  // Clear buffer after each send
    }

    if (n < 0) {
        perror("fread error");
    }

    // Ensure all data is sent before closing
    if (shutdown(client_sock, SHUT_WR) == -1) {
        perror("Shutdown error");
    }

    fclose(file);
    printf("File '%s' sent to client.\n", filename);
}
