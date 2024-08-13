#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

#define PORT 9675
#define BUF_SIZE 1024

int connect_to_server();  // Function prototype

void upload_file(int sock, const char *filename, const char *dest_path);
void download_file(int sock, const char *filename);
void request_tar_file(int sock, const char *filetype);
void request_display(int sock, const char *pathname);
void expand_tilde(char *path, char *expanded_path, size_t size);
int validate_command(const char *command);

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
        printf("Connection closed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Smain server\n");

    while (1) {
        int sock = connect_to_server();
        if (sock == -1) {
            printf("Could not connect to server.\n");
            exit(EXIT_FAILURE);
        }
        printf("Enter command: ");
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character

        // Validate the command before sending it to the server
        if (!validate_command(buffer)) {
            close(sock);
            continue; // Skip this iteration if the command is invalid
        }

        if (strncmp(buffer, "ufile ", 6) == 0) {
            // int sock = connect_to_server(); // Reconnect for each command
            // if (sock == -1) continue;
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
            // Receive and print the response from the server
            ssize_t n = recv(sock, buffer, BUF_SIZE, 0);
            if (n > 0) {
                buffer[n] = '\0';  // Null-terminate the string
                printf("%s", buffer);  // Print the response from the server
            }

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

        }else if (strncmp(buffer, "dtar ", 5) == 0) {

        int sock = connect_to_server(); // Reconnect for each command
        if (sock == -1) continue;
        char *filetype = buffer + 5;
        request_tar_file(sock, filetype);
        }
        else if (strncmp(buffer, "display ", 8) == 0) {
            int sock = connect_to_server(); // Reconnect for each command
            if (sock == -1) continue;
            char *pathname = buffer + 8;
            if (pathname) {
                request_display(sock, pathname);
            } else {
                printf("Invalid command format\n");
            }
        }
        else {
            printf("Invalid command: Please enter either ufile, dfile, rmfile, dtar or display commands.\n");
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
    char expanded_dest_path[BUF_SIZE];
    FILE *file;
    size_t n;

    // Expand ~ in the destination path
    expand_tilde((char *)dest_path, expanded_dest_path, BUF_SIZE);

    // Check if the file exists before sending the command to the server
    if (access(filename, F_OK) == -1) {
        printf("Error: File or folder '%s' does not exist.\n", filename);
        return;
    }

    snprintf(buffer, BUF_SIZE, "ufile\n%s\n%s\n", filename, expanded_dest_path);
    send(sock, buffer, strlen(buffer), 0);
    printf("Sent command: %s %s\n", filename, expanded_dest_path);

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
    char expanded_filename[BUF_SIZE];
    ssize_t n;

    // Expand ~ in the filename path
    expand_tilde((char *)filename, expanded_filename, BUF_SIZE);

    // Send the dfile command to the server
    snprintf(buffer, BUF_SIZE, "dfile %s", expanded_filename);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the initial response from the server
    n = recv(sock, buffer, BUF_SIZE, 0);
    if (n <= 0) {
        perror("Receive error");
        close(sock);
        return;
    }
    buffer[n] = '\0';  // Null-terminate the received data

    // Check if the response contains an error message
    if (strstr(buffer, "Error:") != NULL) {
        printf("%s", buffer);  // Print the error message
        close(sock);
        return;
    }

    // If no error, proceed to create the file for writing
    char *file_name_only = strrchr(expanded_filename, '/');
    if (file_name_only == NULL) {
        file_name_only = (char *)expanded_filename;  // No directory part in filename
    } else {
        file_name_only++;  // Skip the '/'
    }

    FILE *file = fopen(file_name_only, "wb");
    if (file == NULL) {
        perror("File open error");
        close(sock);
        return;
    }

    // Write the initial buffer to the file (since it's part of the file content)
    fwrite(buffer, 1, n, file);

    // Clear the buffer before continuing to receive the rest of the data
    memset(buffer, 0, BUF_SIZE);

    // Receive the remaining file data from the server and write it to the file
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
    close(sock);
}



void request_tar_file(int sock, const char *filetype) {
    char buffer[BUF_SIZE];
    ssize_t n;

    // Send the dtar command to the server
    snprintf(buffer, BUF_SIZE, "dtar %s", filetype);
    send(sock, buffer, strlen(buffer), 0);

    // Determine the correct filename for the tar file
    char tar_filename[BUF_SIZE];
    if (strcmp(filetype, ".c") == 0) {
        snprintf(tar_filename, BUF_SIZE, "cfiles.tar");
    } else if (strcmp(filetype, ".pdf") == 0) {
        snprintf(tar_filename, BUF_SIZE, "pdf.tar");
    } else if (strcmp(filetype, ".txt") == 0){
        snprintf(tar_filename, BUF_SIZE, "text.tar");
    } else {
        snprintf(tar_filename, BUF_SIZE, "%sfiles.tar", filetype + 1);  // Fallback: Create the filename without the dot
    }

    FILE *file = fopen(tar_filename, "wb");
    if (file == NULL) {
        perror("File open error");
        return;
    }

    size_t total_bytes_received = 0;
    while ((n = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        fwrite(buffer, 1, n, file);
        total_bytes_received += n;
        memset(buffer, 0, BUF_SIZE);
    }

    fclose(file);

    if (total_bytes_received == 0) {
        printf("No files exist to create the tar archive.\n");
        remove(tar_filename); // Delete the empty tar file
    } else if (n == 0) {
        printf("Tar file '%s' downloaded successfully.\n", tar_filename);
    } else if (n < 0) {
        perror("Receive error");
    }

    close(sock);
}

void request_display(int sock, const char *pathname) {
    char buffer[BUF_SIZE];
    char expanded_pathname[BUF_SIZE];
    ssize_t n;
    size_t base_len;
    int files_found = 0;  // Variable to track if any files are found

    // Expand ~ in the pathname
    expand_tilde((char *)pathname, expanded_pathname, BUF_SIZE);
    base_len = strlen(expanded_pathname);

    // Send the display command to the server
    snprintf(buffer, BUF_SIZE, "display %s", expanded_pathname);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the list of filenames from the server
    while ((n = recv(sock, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[n] = '\0';  // Null-terminate the received data

        // Process each line in the buffer to strip the base path
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            if (!files_found) {
                printf("List of files in %s:\n", expanded_pathname);  // Print the message only once when the first file is found
                files_found = 1;
            }

            if (strncmp(line, expanded_pathname, base_len) == 0) {
                // Print only the part after the base path
                printf("%s\n", line + base_len + 1);  // +1 to remove the leading slash
            } else if (strstr(line, "/spdf/") != NULL) {
                // Handle spdf files
                char *spdf_base = strstr(line, "/spdf/");
                if (spdf_base) {
                    printf("%s\n", spdf_base + 6);  // +6 to remove "/spdf/"
                }
            } else if (strstr(line, "/stext/") != NULL) {
                // Handle stext files
                char *stext_base = strstr(line, "/stext/");
                if (stext_base) {
                    printf("%s\n", stext_base + 7);  // +7 to remove "/stext/"
                }
            } else {
                // If the line doesn't match any base path, just print it
                printf("%s\n", line);
            }
            line = strtok(NULL, "\n");
        }
    }

    if (files_found == 0) {
        printf("There are no files in %s.\n", expanded_pathname);  // Print if no files are found
    }

    if (n == 0 && files_found) {
        printf("\nDisplay command completed.\n");
    } else if (n < 0) {
        perror("Receive error");
    }

    // Close the socket after finishing the command
    close(sock);
}

// Function to expand ~ to the user's home directory
void expand_tilde(char *path, char *expanded_path, size_t size) {
    if (path[0] == '~') {
        const char *home_dir = getenv("HOME");
        if (!home_dir) {
            struct passwd *pw = getpwuid(getuid());
            home_dir = pw->pw_dir;
        }
        snprintf(expanded_path, size, "%s%s", home_dir, path + 1);
    } else {
        snprintf(expanded_path, size, "%s", path);
    }
}

int validate_command(const char *command) {
    char buffer[BUF_SIZE];
    strncpy(buffer, command, BUF_SIZE);

    char *cmd = strtok(buffer, " ");
    if (!cmd) return 0;

    char *filename;
    char *dest_path;
    char *extra_arg;
    const char *home_dir = getenv("HOME");

    if (strcmp(cmd, "ufile") == 0) {
        // ufile filename destination_path
        filename = strtok(NULL, " ");
        dest_path = strtok(NULL, " ");
        extra_arg = strtok(NULL, " ");

        if (!filename || !dest_path || extra_arg) {
            printf("Usage: ufile <filename> <destination_path>\n");
            return 0;
        }

        // Validate the tilde usage in filename and destination path
        if ((filename[0] == '~' && filename[1] != '/') || (dest_path[0] == '~' && dest_path[1] != '/')) {
            printf("Error: Invalid path. Use '~/smain' or '/home/username/smain' instead.\n");
            return 0;
        }

        // Validate that the path starts with ~/smain or /home/username/smain
        if (!(strncmp(dest_path, "~/smain", 7) == 0 || strncmp(dest_path, home_dir, strlen(home_dir)) == 0) ||
            (strncmp(dest_path, home_dir, strlen(home_dir)) == 0 && strncmp(dest_path + strlen(home_dir), "/smain", 6) != 0)) {
            printf("Error: Path must start with '~/smain' or '/home/username/smain'.\n");
            return 0;
        }

        // Validate file extension
        char *ext = strrchr(filename, '.');
        if (!ext || (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && strcmp(ext, ".txt") != 0)) {
            printf("Invalid extension. Only .c, .pdf, and .txt are allowed.\n");
            return 0;
        }

    } else if (strcmp(cmd, "dfile") == 0 || strcmp(cmd, "rmfile") == 0) {
        // dfile pathname or rmfile filename
        filename = strtok(NULL, " ");
        extra_arg = strtok(NULL, " ");

        if (!filename || extra_arg) {
            printf("Usage: %s <filename>\n", cmd);
            return 0;
        }

        // Validate the tilde usage in filename
        if (filename[0] == '~' && filename[1] != '/') {
            printf("Error: Invalid path. Use '~/smain' or '/home/username/smain' instead.\n");
            return 0;
        }

        // Validate that the path starts with ~/smain or /home/username/smain
        if (!(strncmp(filename, "~/smain", 7) == 0 || strncmp(filename, home_dir, strlen(home_dir)) == 0) ||
            (strncmp(filename, home_dir, strlen(home_dir)) == 0 && strncmp(filename + strlen(home_dir), "/smain", 6) != 0)) {
            printf("Error: Path must start with '~/smain' or '/home/username/smain'.\n");
            return 0;
        }

        // Validate file extension
        char *ext = strrchr(filename, '.');
        if (!ext || (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && strcmp(ext, ".txt") != 0)) {
            printf("Invalid extension. Only .c, .pdf, and .txt are allowed.\n");
            return 0;
        }

    } else if (strcmp(cmd, "dtar") == 0) {
        // dtar filetype
        char *filetype = strtok(NULL, " ");
        extra_arg = strtok(NULL, " ");

        if (!filetype || extra_arg) {
            printf("Usage: dtar <filetype>\n");
            return 0;
        }

        // Validate the tilde usage in filetype
        if (filetype[0] == '~' && filetype[1] != '/') {
            printf("Error: Invalid path. Use '~/smain' or '/home/username/smain' instead.\n");
            return 0;
        }

        if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0) {
            printf("Invalid filetype. Only .c, .pdf, and .txt are allowed.\n");
            return 0;
        }

    } else if (strcmp(cmd, "display") == 0) {
        // display pathname
        char *pathname = strtok(NULL, " ");
        extra_arg = strtok(NULL, " ");

        if (!pathname || extra_arg) {
            printf("Usage: display <pathname>\n");
            return 0;
        }

        // Validate the tilde usage in pathname
        if (pathname[0] == '~' && pathname[1] != '/') {
            printf("Error: Invalid path. Use '~/smain' or '/home/username/smain' instead.\n");
            return 0;
        }

        // Validate that the path starts with ~/smain or /home/username/smain
        if (!(strncmp(pathname, "~/smain", 7) == 0 || strncmp(pathname, home_dir, strlen(home_dir)) == 0) ||
            (strncmp(pathname, home_dir, strlen(home_dir)) == 0 && strncmp(pathname + strlen(home_dir), "/smain", 6) != 0)) {
            printf("Error: Path must start with '~/smain' or '/home/username/smain'.\n");
            return 0;
        }

        // Validate file extension
        char *ext = strrchr(pathname, '.');
        if (ext && strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && strcmp(ext, ".txt") != 0) {
            printf("Invalid extension. Only .c, .pdf, and .txt are allowed.\n");
            return 0;
        }
    } else {
        printf("Unknown command\n");
        return 0;
    }

    return 1; // Command is valid
}
