#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define PORT 8086
#define BUF_SIZE 1024

void handle_client(int client_sock);
void send_to_server(const char *filename, const char *server_ip, int server_port, const char *dest_path);
int make_dirs(const char *path);

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    pid_t child_pid;

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind error");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sock, 10) < 0)
    {
        perror("Listen error");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Smain server listening on port %d\n", PORT);

    while (1)
    {
        // Accept a connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        {
            perror("Accept error");
            continue;
        }

        // Fork a child process to handle the client
        if ((child_pid = fork()) == 0)
        {
            close(server_sock);
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        }
        else if (child_pid < 0)
        {
            perror("Fork error");
            close(client_sock);
        }
        else
        {
            close(client_sock);
            waitpid(-1, NULL, WNOHANG);
        }
    }

    close(server_sock);
    return 0;
}

void handle_client(int client_sock)
{
    char buffer[BUF_SIZE];
    ssize_t n;

    while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("Received command: %s\n", buffer);

        // Parse command and handle accordingly
        if (strncmp(buffer, "ufile ", 6) == 0)
        {
            // Handle file upload
            char *filename = strtok(buffer + 6, " ");
            char *dest_dir = strtok(NULL, " ");
            if (filename && dest_dir)
            {
                // Check file extension
                char *ext = strrchr(filename, '.');
                if (ext)
                {
                    if (strcmp(ext, ".c") == 0)
                    {
                        // Store .c file locally
                        if (make_dirs(dest_dir) != 0)
                        {
                            perror("mkdir error");
                            return;
                        }
                        char fullpath[BUF_SIZE];
                        snprintf(fullpath, BUF_SIZE, "%s/%s", dest_dir, filename);
                        int file_fd = open(fullpath, O_WRONLY | O_CREAT, 0644);
                        if (file_fd < 0)
                        {
                            perror("File open error");
                            return;
                        }

                        // Read the file from the current directory and write it to the destination
                        FILE *source_file = fopen(filename, "rb");
                        if (source_file == NULL)
                        {
                            perror("Source file open error");
                            close(file_fd);
                            return;
                        }
                        while ((n = fread(buffer, 1, BUF_SIZE, source_file)) > 0)
                        {
                            write(file_fd, buffer, n);
                        }
                        fclose(source_file);
                        close(file_fd);
                    }
                    else if (strcmp(ext, ".pdf") == 0)
                    {
                        // Modify the destination directory to replace "smain" with "spdf"
                        char modified_dest_dir[BUF_SIZE];
                        snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_dir);
                        char *replace = strstr(modified_dest_dir, "smain");
                        if (replace)
                        {
                            memmove(replace + 4, replace + 5, strlen(replace + 5) + 1); // Shift the rest of the string left by 1 character
                            strncpy(replace, "spdf", 4);
                        }
                        // Transfer .pdf file to Spdf server
                        send_to_server(filename, "127.0.0.1", 8081, modified_dest_dir);
                    }
                    else if (strcmp(ext, ".txt") == 0)
                    {
                        // Modify the destination directory to replace "smain" with "stext"
                        char modified_dest_dir[BUF_SIZE];
                        snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_dir);
                        char *replace = strstr(modified_dest_dir, "smain");
                        if (replace)
                        {
                            memmove(replace + 5, replace + 5, strlen(replace + 5) + 1); // Shift the rest of the string left by 1 character
                            strncpy(replace, "stext", 5);
                        }
                        // Transfer .txt file to Stext server
                        send_to_server(filename, "127.0.0.1", 8082, modified_dest_dir);
                    }
                }
            }
        }
        else if (strncmp(buffer, "dfile ", 6) == 0)
        {
            // Handle file download (similar logic to be implemented)
        }
        else if (strncmp(buffer, "rmfile ", 7) == 0)
        {
            // Handle file removal (similar logic to be implemented)
        }
        else if (strncmp(buffer, "dtar ", 5) == 0)
        {
            // Handle tar file creation (similar logic to be implemented)
        }
        else if (strncmp(buffer, "display ", 8) == 0)
        {
            // Handle display files (similar logic to be implemented)
        }
    }

    if (n < 0)
    {
        perror("Recv error");
    }
}

void send_to_server(const char *filename, const char *server_ip, int server_port, const char *dest_dir)
{
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    FILE *file;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Open file for reading
    if ((file = fopen(filename, "rb")) == NULL)
    {
        perror("File open error");
        close(sock);
        return;
    }

    // Send destination directory first
    send(sock, dest_dir, strlen(dest_dir), 0);

    // Send filename next
    send(sock, filename, strlen(filename), 0);

    // Send file data
    while (!feof(file))
    {
        size_t n = fread(buffer, 1, BUF_SIZE, file);
        if (send(sock, buffer, n, 0) < 0)
        {
            perror("Send error");
            break;
        }
    }

    fclose(file);
    close(sock);
}

int make_dirs(const char *path)
{
    char tmp[BUF_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, 0755) && errno != EEXIST)
            {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) && errno != EEXIST)
    {
        return -1;
    }
    return 0;
}
