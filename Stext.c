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

#define PORT 8082
#define BUF_SIZE 1024

void handle_client(int client_sock);
int make_dirs(const char *path);

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

    printf("Stext server listening on port %d\n", PORT);

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
    FILE *file;
    char dest_path[BUF_SIZE];
    char filename[BUF_SIZE];

    // Receive destination path
    n = recv(client_sock, dest_path, BUF_SIZE, 0);
    if (n <= 0) {
        perror("Recv error");
        return;
    }
    dest_path[n] = '\0';

    // Receive filename
    n = recv(client_sock, filename, BUF_SIZE, 0);
    if (n <= 0) {
        perror("Recv error");
        return;
    }
    filename[n] = '\0';

    // Print received message
    printf("Request received to store file '%s' in directory '%s'\n", filename, dest_path);

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

    // Receive file data
    while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
        fwrite(buffer, 1, n, file);
    }

    if (n < 0) {
        perror("Recv error");
    }

    fclose(file);

    // Print success message
    printf("File '%s' successfully stored in directory '%s'\n", filename, dest_path);
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
