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

#define PORT 8096
#define BUF_SIZE 1024

void setup_server();
void handle_connection(int client_sock);
void handle_client(int client_sock, const char *command);
void handle_ufile(const char *filename, const char *dest_path, int client_sock);
void handle_other_commands(const char *command, int client_sock);  // Placeholder for other commands
void send_to_server(const char *filename, const char *server_ip, int server_port, const char *dest_dir, int client_sock);
int make_dirs(const char *path);
void handle_rmfile(const char *filename, int client_sock);
void send_delete_request(const char *filename, const char *server_ip, int server_port);
void replace_smain_path(char *path, const char *replacement);
void retrieve_and_send_file(const char *filename, int client_sock);
void request_file_from_server(const char *filename, const char *server_ip, int server_port, int client_sock);
void handle_dfile(const char *filename, int client_sock);

int main() {
    setup_server();
    return 0;
}

// Function to set up the server and manage incoming connections
void setup_server() {
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

    printf("Smain server listening on port %d\n", PORT);

    while (1) {
        // Accept a connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept error");
            continue;
        }

        // Fork a child process to handle the client
        if ((child_pid = fork()) == 0) {
            close(server_sock);
            handle_connection(client_sock);
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
}

void handle_connection(int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;

    while (1) {  // Keep the connection open for multiple commands
        // Clear the buffer before receiving a new command
        memset(buffer, 0, BUF_SIZE);

        // Receive the command from the client
        n = recv(client_sock, buffer, BUF_SIZE, 0);
        if (n <= 0) {
            if (n == 0) {
                printf("Client disconnected\n");
            } else {
                perror("Recv error");
            }
            break;  // Exit the loop if the client disconnects or an error occurs
        }
        buffer[n] = '\0';
        printf("Received command: %s\n", buffer);

        // Call handle_client to process the command
        handle_client(client_sock, buffer);
    }

    // Close the connection when the loop ends (client disconnects)
    close(client_sock);
}



// Function to handle a single command from the client
void handle_client(int client_sock, const char *command) {
    char *cmd = strtok((char *)command, "\n");

    if (cmd == NULL) {
        printf("Invalid command\n");
        return;
    }
    if (strncmp(cmd, "ufile", 5) == 0) {
        char *received_filename = strtok(NULL, "\n");
        char *received_dest_path = strtok(NULL, "\n");
        if (!received_filename || !received_dest_path) {
            printf("Invalid ufile command format\n");
            return;  // Exit if the command format is invalid
        }
        handle_ufile(received_filename, received_dest_path, client_sock);
    } else if (strncmp(cmd, "rmfile", 6) == 0) {
    // Parse the filename after "rmfile "
    char *received_filename = cmd + 7;
    if (!received_filename || strlen(received_filename) == 0) {
        printf("Invalid rmfile command format\n");
        return;
    }
    handle_rmfile(received_filename, client_sock);
    }
    else if (strncmp(cmd, "dfile", 5) == 0) {
        // Parse the filename after "dfile "
        char *received_filename = cmd + 6;
        if (!received_filename || strlen(received_filename) == 0) {
            printf("Invalid dfile command format\n");
            return;
        }
        handle_dfile(received_filename, client_sock);
    }
    else {
        handle_other_commands(cmd, client_sock);  // Placeholder for handling other commands
    }
}

// Function to handle the "ufile" command
void handle_ufile(const char *filename, const char *dest_path, int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;

    // Determine file extension
    char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcmp(ext, ".c") == 0) {
            // Store .c file locally
            if (make_dirs(dest_path) != 0) {
                perror("mkdir error");
                return;
            }
            char fullpath[BUF_SIZE];
            snprintf(fullpath, BUF_SIZE, "%s/%s", dest_path, filename);
            FILE *file = fopen(fullpath, "wb");
            if (!file) {
                perror("File open error");
                return;
            }

            // Now receive and write the file data
            while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
                size_t written = fwrite(buffer, 1, n, file);
                if (written != n) {
                    perror("fwrite error");
                    break;
                }
                fflush(file);
            }

            fclose(file);

        } else if (strcmp(ext, ".pdf") == 0) {
            // Handle .pdf file, modify path and send to Spdf server
            char modified_dest_dir[BUF_SIZE];
            snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_path);
            char *replace = strstr(modified_dest_dir, "smain");
            if (replace) {
                memmove(replace + 4, replace + 5, strlen(replace + 5) + 1);
                strncpy(replace, "spdf", 4);
            }
            if (make_dirs(modified_dest_dir) != 0) {
                perror("mkdir error");
                return;
            }
            send_to_server(filename, "127.0.0.1", 8081, modified_dest_dir, client_sock);

        } else if (strcmp(ext, ".txt") == 0) {
            // Handle .txt file, modify path and send to Stext server
            char modified_dest_dir[BUF_SIZE];
            snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_path);
            char *replace = strstr(modified_dest_dir, "smain");
            if (replace) {
                memmove(replace + 5, replace + 5, strlen(replace + 5) + 1);
                strncpy(replace, "stext", 5);
            }
            if (make_dirs(modified_dest_dir) != 0) {
                perror("mkdir error");
                return;
            }
            send_to_server(filename, "127.0.0.1", 8086, modified_dest_dir, client_sock);
        }
    }
}

void handle_rmfile(const char *filename, int client_sock) {
    char modified_filename[BUF_SIZE];

    // Check if the file is a .txt file
    if (strstr(filename, ".txt") != NULL) {
        // Modify the path to use the "stext" directory instead of "smain"
        strncpy(modified_filename, filename, BUF_SIZE);

        char *replace = strstr(modified_filename, "/smain/");
        if (replace) {
            // Replace "/smain/" with "/stext/"
            strncpy(replace, "/stext/", 7);
        }

        // Send the modified path to the Stext server
        send_delete_request(modified_filename, "127.0.0.1", 8086);

    } else if (strstr(filename, ".pdf") != NULL) {
        // Modify the path to use the "spdf" directory instead of "smain"
        strncpy(modified_filename, filename, BUF_SIZE);

        char *replace = strstr(modified_filename, "/smain/");
        if (replace) {
            // Replace "/smain/" with "/spdf/"
            strncpy(replace, "/spdf/", 6);
        }

        // Send the modified path to the Spdf server
        send_delete_request(modified_filename, "127.0.0.1", 8081);

    } else {
        // Send the original path for non-.txt and non-.pdf files
        send_delete_request(filename, "127.0.0.1", 8081); // Adjust if needed
    }
}


// Placeholder function for handling other commands
void handle_other_commands(const char *command, int client_sock) {
    printf("Handling other command: %s\n", command);
    // Add logic here to handle other types of commands
}

void send_to_server(const char *filename, const char *server_ip, int server_port, const char *dest_dir, int client_sock) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send filename and destination directory first
    snprintf(buffer, BUF_SIZE, "%s\n%s\n", filename, dest_dir);
    send(sock, buffer, strlen(buffer), 0);

    // Send file data
    while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
        send(sock, buffer, n, 0);
    }

    close(sock);
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

void send_delete_request(const char *filename, const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send the delete command and filename
    snprintf(buffer, BUF_SIZE, "rmfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    close(sock);
}

void replace_smain_path(char *path, const char *replacement) {
    char *replace = strstr(path, "/smain/");
    if (replace) {
        memmove(replace + strlen(replacement), replace + strlen("/smain/"), strlen(replace) - strlen("/smain/") + 1);
        memcpy(replace, replacement, strlen(replacement));
    }
}

void retrieve_and_send_file(const char *filename, int client_sock) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("File open error");
        return;
    }

    char buffer[BUF_SIZE];
    ssize_t n;

    // Ensure the buffer is clean before starting the transfer
    memset(buffer, 0, BUF_SIZE);

    // Read and send the file content to the client
    while ((n = fread(buffer, 1, BUF_SIZE, file)) > 0) {
        if (send(client_sock, buffer, n, 0) == -1) {
            perror("Send error");
            break;
        }
        memset(buffer, 0, BUF_SIZE);  // Clear buffer after each send to ensure no residual data
    }

    if (n < 0) {
        perror("fread error");
    }

    // Properly shut down the connection after sending all data
    shutdown(client_sock, SHUT_WR);  // Signal to client that we're done sending data

    fclose(file);
    printf("File '%s' sent to client.\n", filename);
}

void request_file_from_server(const char *filename, const char *server_ip, int server_port, int client_sock) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send the filename to the server
    snprintf(buffer, BUF_SIZE, "dfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the file data from the server and forward it to the client immediately
    while ((n = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        if (send(client_sock, buffer, n, 0) == -1) {
            perror("Forwarding error");
            break;
        }
    }

    if (n < 0) {
        perror("Receive error from server");
    }

    // Properly shut down the socket after sending all data
    shutdown(sock, SHUT_WR);  // Ensure the server closes the sending side
    close(sock);
    
    // Signal the client that the data transmission is complete
    shutdown(client_sock, SHUT_WR);  // Close the writing side of the client socket
}



void handle_dfile(const char *filename, int client_sock) {
    char file_path[BUF_SIZE];
    strncpy(file_path, filename, BUF_SIZE);

    // Check the file type and handle accordingly
    if (strstr(file_path, ".c") != NULL) {
        // Process .c file locally
        retrieve_and_send_file(file_path, client_sock);
    } else if (strstr(file_path, ".txt") != NULL) {
        // Replace smain with stext and request the file from Stext
        replace_smain_path(file_path, "/stext/");
        request_file_from_server(file_path, "127.0.0.1", 8086, client_sock);
    } else if (strstr(file_path, ".pdf") != NULL) {
        // Replace smain with spdf and request the file from Spdf
        replace_smain_path(file_path, "/spdf/");
        request_file_from_server(file_path, "127.0.0.1", 8081, client_sock);
    } else {
        printf("Unsupported file type\n");
    }
}
