#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 8096
#define BUF_SIZE 1024

void upload_file(int sock, const char *filename, const char *dest_path);
void download_file(int sock, const char *filename);

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to Smain server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Smain server\n");

    while (1) {
        printf("Enter command: ");
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character

        if (strncmp(buffer, "ufile ", 6) == 0) {
            char *filename = strtok(buffer + 6, " ");
            char *dest_path = strtok(NULL, " ");
            if (filename && dest_path) {
                upload_file(sock, filename, dest_path);
            } else {
                printf("Invalid command format\n");
            }
        } else if (strncmp(buffer, "rmfile ", 7) == 0) {
            // Send the rmfile command to the server
            send(sock, buffer, strlen(buffer), 0);
        } else if (strncmp(buffer, "dfile ", 6) == 0) {
        int sock = connect_to_server(); // Reconnect for each command
        if (sock == -1) continue;

        char *filename = buffer + 6;
        if (filename) {
            download_file(sock, filename);
        } else {
            printf("Invalid command format\n");
        }
        close(sock); // Close connection after download

        } else {
            printf("Unknown command\n");
        }
    }

    close(sock);
    return 0;
}

int connect_to_server() {
    int sock;
    struct sockaddr_in server_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}


void upload_file(int sock, const char *filename, const char *dest_path) {
    char buffer[BUF_SIZE];
    FILE *file;
    size_t n;

    snprintf(buffer, BUF_SIZE, "ufile\n%s\n%s\n", filename, dest_path);
    send(sock, buffer, strlen(buffer), 0);
    printf("Sent command: %s %s\n", filename, dest_path);

    if ((file = fopen(filename, "rb")) == NULL) {
        perror("File open error");
        return;
    }

    while ((n = fread(buffer, 1, BUF_SIZE, file)) > 0) {
        send(sock, buffer, n, 0);
    }

    fclose(file);
    shutdown(sock, SHUT_WR); // Close the write side of the socket to signal EOF
}

void download_file(int sock, const char *filename) {
    char buffer[BUF_SIZE];
    ssize_t n;

    // Send the dfile command to the server
    snprintf(buffer, BUF_SIZE, "dfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    // Open the file for writing in the current directory
    char *file_name_only = strrchr(filename, '/');
    if (file_name_only == NULL) {
        file_name_only = (char *)filename;  // No directory part in filename
    } else {
        file_name_only++;  // Skip the '/'
    }

    FILE *file = fopen(file_name_only, "wb");
    if (file == NULL) {
        perror("File open error");
        return;
    }

    // Clear the buffer before starting the data transfer
    memset(buffer, 0, BUF_SIZE);

    // Receive the file data from the server and write it to the file
    while ((n = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        fwrite(buffer, 1, n, file);
        memset(buffer, 0, BUF_SIZE);  // Clear buffer after each write to avoid leftover data
    }

    if (n == 0) {
        // This means the server has closed the connection (end of file transmission)
        printf("File '%s' downloaded successfully.\n", file_name_only);
    } else if (n < 0) {
        perror("Receive error");
    }

    fclose(file);
}
