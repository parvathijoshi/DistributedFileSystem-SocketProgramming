// COMP 8567 - Advanced Systems Programming
// Final Project - Distributed File System using Socket Programming
// Team members: 
// Parvathi Puthedath Joshy -110146653
// Ardra Sanjiv Kumar - 110129179
//------------------------------------------------------------------
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
#include <sys/wait.h>

#define PORT 9800
#define BUF_SIZE 1024

// Function declarations
void handleCommandsfromClient(int client_sock);
void rmfileCommandExecution(const char *filename, int client_sock);
void ufileCommandExecution(const char *filename, const char *dest_path, const char *file_content, int client_sock);
int createDir(const char *path);
void dfileCommandExecution(const char *filename, int client_sock);
void dtarCommandExecution(int client_sock);
void displayCommandExecution(const char *directory, int client_sock);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    pid_t child_pid;

    // Create a socket for the server
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    // Set the ipv4 address and port 
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
            // Child closes the server socket
	    close(server_sock);
            handleCommandsfromClient(client_sock);
            close(client_sock);
            exit(0);
        } else if (child_pid < 0) { // Fork error
            perror("Fork error");
            close(client_sock);
        } else { // Parent closes the client socket
            close(client_sock);
            waitpid(-1, NULL, WNOHANG);  //  Non-blocking wait for child to rerminate
        }
    }

    close(server_sock);
    return 0;
}

// Function to handle commands from the client
void handleCommandsfromClient(int client_sock) {
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
    buffer[n] = '\0'; // Null-terminate the received data
    printf("Received command: %s\n", buffer);

    // Option handling for dfile, display, rmfile, ufile
    if (strncmp(buffer, "dfile ", 6) == 0) {
        char *received_filename = buffer + 6;

        // Trim any newline characters
        received_filename[strcspn(received_filename, "\n")] = 0;

        // If the request is for txt.tar, ensure it is created before sending
        if (strcmp(received_filename, "text.tar") == 0) {
            dtarCommandExecution(client_sock);
        } else {
            dfileCommandExecution(received_filename, client_sock);
        }

    } else if (strncmp(buffer, "display", 7) == 0) {
    	char *directory = buffer + 8;
    	displayCommandExecution(directory, client_sock);
    }
     else if (strncmp(buffer, "rmfile ", 7) == 0) {
        char *received_filename = buffer + 7;
        rmfileCommandExecution(received_filename, client_sock);
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
        ufileCommandExecution(received_filename, received_dest_path, received_file_content, client_sock);
    }
    close(client_sock);
}

// Function to execute the rmfile command
void rmfileCommandExecution(const char *filename, int client_sock) {
    char response[BUF_SIZE];

    // Perform the file deletion
    if (remove(filename) == 0) {
        snprintf(response, BUF_SIZE, "File is deleted successfully.\n");
        printf("File '%s' deleted successfully.\n", filename);
    } else {
        snprintf(response, BUF_SIZE, "File deletion error: %s\n", strerror(errno));
    }

    // Send the response back to the client
    send(client_sock, response, strlen(response), 0);
}

// Function to execute the ufile command
void ufileCommandExecution(const char *filename, const char *dest_path, const char *file_content, int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;
    FILE *file;

    // Create the directory if it doesn't exist
    if (createDir(dest_path) != 0) {
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

// Function to create the directory 
int createDir(const char *path) {
    char tmp[BUF_SIZE];
    char *p = NULL;
    size_t len;
    
    // Copy the path to a temporary buffer
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

// Function to execute the dfile command
void dfileCommandExecution(const char *filename, int client_sock) {
    // Check if the file exists
    if (access(filename, F_OK) == -1) {
        char response[BUF_SIZE];
        snprintf(response, BUF_SIZE, "Error: File/Directory does not exist.\n");
        send(client_sock, response, strlen(response), 0);
        return;
    }
    
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
    printf("File '%s' sent to Smain.\n", filename);
}

// Function to execute the dtar command
void dtarCommandExecution(int client_sock) {
    char command[BUF_SIZE];
    char home_dir[BUF_SIZE];
    int pipefd[2];
    pid_t pid;

    // Get the user's home directory
    const char *home = getenv("HOME");
    if (!home) {
        perror("Unable to get the home directory");
        return;
    }

    // Construct the path to the stext directory under the home directory
    snprintf(home_dir, sizeof(home_dir), "%s/stext", home);

    // Create the command to tar all .txt files in the ~/stext directory
    snprintf(command, BUF_SIZE, "cd %s && tar -cvf - $(find . -type f -name '*.txt')", home_dir);

    // Create a pipe to capture the output of the tar command
    if (pipe(pipefd) == -1) {
        perror("pipe error");
        return;
    }

    // Fork a process to run the tar command
    if ((pid = fork()) == -1) {
        perror("fork error");
        return;
    } else if (pid == 0) {
        // Child runs the tar command
        close(pipefd[0]);  // Close the read end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
        close(pipefd[1]);  // Close the write end after redirect

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("execl error");  // If execl fails
        exit(EXIT_FAILURE);
    } else {
        // Parent reads the tar output from the pipe and send it over the socket
        close(pipefd[1]);  // Close the write end of the pipe

        char buffer[BUF_SIZE];
        ssize_t n;

        // Read the tar output and send it to the client
        while ((n = read(pipefd[0], buffer, BUF_SIZE)) > 0) {
            if (send(client_sock, buffer, n, 0) == -1) {
                perror("send error");
                break;
            }
        }

        if (n < 0) {
            perror("read error");
        }

        close(pipefd[0]);  // Close the read end of the pipe
        wait(NULL);  // Wait for the child to finish
    }

    // Properly shut down the connection after sending all data
    shutdown(client_sock, SHUT_WR);
    printf("Tar file sent to client directly from %s directory.\n", home_dir);
}

// Function to execute the display command
void displayCommandExecution(const char *directory, int client_sock) {
    char buffer[BUF_SIZE];
    char cmd[BUF_SIZE];
    FILE *fp;

    // Print the directory being searched
    printf("Searching for .txt files in directory: %s\n", directory);

    // Construct the command to find .pdf files in the specified directory
    snprintf(cmd, sizeof(cmd), "find %s -type f -name '*.txt'", directory);
    
    // Print the command being executed
    printf("Executing command: %s\n", cmd);

    // Open a pipe to read the output of the command
    fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        close(client_sock);
        return;
    }

    // Read the file names and send them to Smain
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        send(client_sock, buffer, strlen(buffer), 0);
        // Debug: Print each file sent
        printf("Sending file: %s", buffer);
    }

    pclose(fp);

    // Properly close the write side of the socket to signal end of data
    shutdown(client_sock, SHUT_WR);
    printf("Completed handling display command and sent file list.\n");
}
