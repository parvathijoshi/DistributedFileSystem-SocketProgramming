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
#include <sys/wait.h>  
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>

#define PORT 9678
#define BUF_SIZE 1024

//Function declarations
void prcclient();
void handleClientConnection(int client_sock);
void handleCommandsfromClient(int client_sock, const char *command);
void ufileCommandExecution(const char *filename, const char *dest_path, int client_sock);
void sendFileandPathtoServer(const char *filename, const char *server_ip, int server_port, const char *dest_dir, int client_sock);
int createDir(const char *path);
void rmfileCommandExecution(const char *filename, int client_sock);
void sendRemoveRequesttoServer(const char *filename, const char *server_ip, int server_port, int client_sock);
void replacesmainPath(char *path, const char *replacement);
void retrieveAndSendFile(const char *filename, int client_sock);
void requestFileFromServer(const char *filename, const char *server_ip, int server_port, int client_sock);
void dfileCommandExecution(const char *filename, int client_sock);
void requestFileListFromServer(const char *server_ip, int server_port, const char *command, const char *directory, char *file_list);
void dtarCommandExecution(const char *filetype, int client_sock);
void displayCommandExecution(const char *pathname, int client_sock);
void tildePathOperation(char *path, char *expanded_path, size_t size);
void collectFiles(const char *directory, const char *filetype, char *output);

int main() {
    //Start the server
    prcclient();
    return 0;
}

// Function to set up the server and manage incoming connections
void prcclient() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    pid_t child_pid;

    // Create a socket using IPv4 and TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Set up the address structure for the server
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
    // Infinite loop to accept multiple client connections
    while (1) {
        // Accept a connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept error");
            continue;
        }

        // Fork a child process to handle the client
        if ((child_pid = fork()) == 0) {
            // Child process closes listening socket
            close(server_sock);
            // Handle client communication by calling the function
            handleClientConnection(client_sock);
            close(client_sock);
            exit(0);
        } else if (child_pid < 0) {
            perror("Fork error");
            close(client_sock);
        } else {
            // Parent process closes client socket
            close(client_sock);
            // Clean up zombie processes
            waitpid(-1, NULL, WNOHANG);
        }
    }
    close(server_sock);
}

// Function to handle client connections
void handleClientConnection(int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;

    while (1) {  // Keep the connection open for multiple commands
        // Clear the buffer before receiving a new command
        memset(buffer, 0, BUF_SIZE);

        // Receive the command from the client
        n = recv(client_sock, buffer, BUF_SIZE, 0);
        if (n <= 0) {
            if (n != 0) {
                printf("Recv error\n");
            } 
            break;  // Exit the loop if the client disconnects or an error occurs
        }
        buffer[n] = '\0';
        printf("Received command: %s\n", buffer);

        // Call handleCommandsfromClient to process the command
        handleCommandsfromClient(client_sock, buffer);
    }

    // Close the connection when the loop ends (client disconnects)
    close(client_sock);
}

// Function to handle a single command from the client
void handleCommandsfromClient(int client_sock, const char *command) {
    //Getting the command
    char *cmd = strtok((char *)command, "\n");
    //Validation for invalid commands
    if (cmd == NULL) {
        printf("Invalid command\n");
        return;
    }
    //Option handling for the ufile command
    if (strncmp(cmd, "ufile", 5) == 0) {
        char *received_filename = strtok(NULL, "\n");
        char *received_dest_path = strtok(NULL, "\n");
        if (!received_filename || !received_dest_path) {
            printf("Invalid ufile command format\n");
            return;  // Exit if the command format is invalid
        }
        //Calling the function if the validation is successful
        ufileCommandExecution(received_filename, received_dest_path, client_sock);
    } 
    //Option handling for the rmfile command
    else if (strncmp(cmd, "rmfile", 6) == 0) {
    // Parse the filename after "rmfile "
    char *received_filename = cmd + 7;
    if (!received_filename || strlen(received_filename) == 0) {
        printf("Invalid rmfile command format\n");
        return;
    }
    //Calling the function if the validation is successful
    rmfileCommandExecution(received_filename, client_sock);
    }
    //Option handling for the dfile command
    else if (strncmp(cmd, "dfile", 5) == 0) {
        // Parse the filename after "dfile "
        char *received_filename = cmd + 6;
        if (!received_filename || strlen(received_filename) == 0) {
            printf("Invalid dfile command format\n");
            return;
        }
        //Calling the function if the validation is successful
        dfileCommandExecution(received_filename, client_sock);
    }
    //Option handling for the dfile command
    else if (strncmp(cmd, "dtar", 4) == 0) {
    char *filetype = cmd + 5;  // Skip the "dtar " part
    //Calling the function
    dtarCommandExecution(filetype, client_sock);
    }
    //Option handling for the display command
    else if (strncmp(cmd, "display", 7) == 0) {
        char *pathname = cmd + 8;
        if (!pathname || strlen(pathname) == 0) {
            printf("Invalid display command format\n");
            return;
        }
        //Calling the function if the validation is successful
        displayCommandExecution(pathname, client_sock);
    }
}

// Function to handle the "ufile" command
void ufileCommandExecution(const char *filename, const char *dest_path, int client_sock) {
    char buffer[BUF_SIZE];
    ssize_t n;

    // Determine file extension
    char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcmp(ext, ".c") == 0) {
            // Creating the directory if it doesnt exists, to store .c files locally
            if (createDir(dest_path) != 0) {
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
            // Receive the file data from client24s and write the data to the file
            while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
                size_t written = fwrite(buffer, 1, n, file);
                if (written != n) {
                    perror("fwrite error");
                    break;
                }
                // Flush the output buffer
                fflush(file);
            }

            fclose(file);

        } else if (strcmp(ext, ".pdf") == 0) {
            // Handle .pdf file, modify path and send to Spdf server
            char modified_dest_dir[BUF_SIZE];
            snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_path);
            // Replacing the smain with spdf before sending it to Spdf server
            char *replace = strstr(modified_dest_dir, "smain");
            if (replace) {
                memmove(replace + 4, replace + 5, strlen(replace + 5) + 1);
                strncpy(replace, "spdf", 4);
            }
            // Creating the directory if it doesn't exists
            if (createDir(modified_dest_dir) != 0) {
                perror("mkdir error");
                return;
            }
            // Sending the file and path to Spdf server
            sendFileandPathtoServer(filename, "127.0.0.1", 9801, modified_dest_dir, client_sock);

        } else if (strcmp(ext, ".txt") == 0) {
            // Handle .txt file, modify path and send to Stext server
            char modified_dest_dir[BUF_SIZE];
            snprintf(modified_dest_dir, BUF_SIZE, "%s", dest_path);
            // Replacing the smain with stext before sending it to Stext server
            char *replace = strstr(modified_dest_dir, "smain");
            if (replace) {
                memmove(replace + 5, replace + 5, strlen(replace + 5) + 1);
                strncpy(replace, "stext", 5);
            }
            // Creating the directory if it doesn't exists
            if (createDir(modified_dest_dir) != 0) {
                perror("mkdir error");
                return;
            }
            // Sending the file and path to Stext server
            sendFileandPathtoServer(filename, "127.0.0.1", 9800, modified_dest_dir, client_sock);
        }
    }
}

// Function to handle the "rmfile" command, which removes a file from the servers
void rmfileCommandExecution(const char *filename, int client_sock) {
    char expanded_filename[BUF_SIZE];
    char modified_filename[BUF_SIZE];
    char response[BUF_SIZE];

    // Expand ~ to the full home directory path
    tildePathOperation((char *)filename, expanded_filename, BUF_SIZE);

    // Check if the file is a .txt file
    if (strstr(expanded_filename, ".txt") != NULL) {
        // Modify the path to use the "stext" directory instead of "smain"
        strncpy(modified_filename, expanded_filename, BUF_SIZE);

        char *replace = strstr(modified_filename, "/smain/");
        if (replace) {
            // Replace "/smain/" with "/stext/"
            snprintf(replace, BUF_SIZE - (replace - modified_filename), "/stext/%s", replace + 7);
        }
        // Send the modified path to the Stext server
        sendRemoveRequesttoServer(modified_filename, "127.0.0.1", 9800, client_sock);

    }
    // Check if the file is a .pdf file
    else if (strstr(expanded_filename, ".pdf") != NULL) {
        // Modify the path to use the "spdf" directory instead of "smain"
        strncpy(modified_filename, expanded_filename, BUF_SIZE);

        char *replace = strstr(modified_filename, "/smain/");
        if (replace) {
            // Replace "/smain/" with "/spdf/"
            snprintf(replace, BUF_SIZE - (replace - modified_filename), "/spdf/%s", replace + 7);
        }

        // Send the modified path to the Spdf server
        sendRemoveRequesttoServer(modified_filename, "127.0.0.1", 9801, client_sock);

    }
    // Check if the file is a .c file
    else if (strstr(expanded_filename, ".c") != NULL) {
        // Directly delete the .c file from the Smain server
        if (remove(expanded_filename) == 0) {
            snprintf(response, BUF_SIZE, "File is deleted successfully.\n", expanded_filename);
        } else {
            snprintf(response, BUF_SIZE, "File deletion error: %s\n", strerror(errno));
        }
    }
    // Send the response to the client
    send(client_sock, response, strlen(response), 0);
}

// Function to send a file and its path to another server
void sendFileandPathtoServer(const char *filename, const char *server_ip, int server_port, const char *dest_dir, int client_sock) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    // Create a socket to connect to the other server
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }
    // Set up the address structure for the other server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }
    // Connect to the other servers
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send filename and destination directory first
    snprintf(buffer, BUF_SIZE, "%s\n%s\n", filename, dest_dir);
    send(sock, buffer, strlen(buffer), 0);

    // Send the file data from the client to the servers
    while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
        send(sock, buffer, n, 0);
    }
    // Close the socket to the servers when done
    close(sock);
}

// Function to create a directory if it does not exist
int createDir(const char *path) {
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
    // Create the final directory in the path
    if (mkdir(tmp, 0755) && errno != EEXIST) {
        return -1;
    }
    return 0;
}

// Function to send a remove request to another server
void sendRemoveRequesttoServer(const char *filename, const char *server_ip, int server_port, int client_sock) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    // Create a socket to connect to the servers
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    // Set up the address structure for the other server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    // Connect to the servers
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send the rmfile (delete) command and filename to the other server
    snprintf(buffer, BUF_SIZE, "rmfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the response from the servers
    while ((n = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        buffer[n] = '\0';  // Null-terminate the string
        send(client_sock, buffer, n, 0);  // Forward the response to the client
    }

    close(sock);
}

// Function to replace part of a file path with a different directory name
void replacesmainPath(char *path, const char *replacement) {
    char *replace = strstr(path, "/smain/");
    if (replace) {
        memmove(replace + strlen(replacement), replace + strlen("/smain/"), strlen(replace) - strlen("/smain/") + 1);
        memcpy(replace, replacement, strlen(replacement));
    }
}

// Function to retrieve a file from the server and send it to the client
void retrieveAndSendFile(const char *filename, int client_sock) {
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

// Function to request a file from servers
void requestFileFromServer(const char *filename, const char *server_ip, int server_port, int client_sock) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    // Create a socket to connect to the servers
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }
    
    // Set up the address structure for the servers
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    // Connect to the servers
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection closed");
        close(sock);
        return;
    }

    // Send the download request of the filename to the server
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
    // Properly shut down the socket after sending all the data
    shutdown(sock, SHUT_WR);  // Ensure the server closes the sending side
    close(sock);
    
    // Signal the client that the data transmission is complete
    shutdown(client_sock, SHUT_WR);  // Close the writing side of the client socket
}

// Function to handle the dfile command, which downloads a file from the servers
void dfileCommandExecution(const char *filename, int client_sock) {
    char file_path[BUF_SIZE];
    strncpy(file_path, filename, BUF_SIZE);

    // Check if the file type is .c
    if (strstr(file_path, ".c") != NULL) {
        // Check if the file/folder exists before proceeding
        if (access(file_path, F_OK) == -1) {
        char response[BUF_SIZE];
        snprintf(response, BUF_SIZE, "Error: File/Directory does not exist.\n");
        //Sending the response to client if the file/directory doesnt exists.
        send(client_sock, response, strlen(response), 0);
        // Properly shut down the write side of the socket to signal the end of the communication
        shutdown(client_sock, SHUT_WR);
        return;
        }
        // Process .c file locally
        retrieveAndSendFile(file_path, client_sock);
    }
    // Check if the file type is .txt
    else if (strstr(file_path, ".txt") != NULL) {
        // Replace smain with stext and request the file from Stext
        replacesmainPath(file_path, "/stext/");
        requestFileFromServer(file_path, "127.0.0.1", 9800, client_sock);
    } 
    // Check if the file type is .pdf
    else if (strstr(file_path, ".pdf") != NULL) {
        // Replace smain with spdf and request the file from Spdf
        replacesmainPath(file_path, "/spdf/");
        requestFileFromServer(file_path, "127.0.0.1", 9801, client_sock);
    } else {
        printf("Unsupported file type\n");
    }
}

// Function to handle the "dtar" command, which sends a tar archive of files to the client
void dtarCommandExecution(const char *filetype, int client_sock) {
    char command[BUF_SIZE];
    char cwd[BUF_SIZE];
    int pipefd[2];
    pid_t pid;

    // Get the user's home directory
    const char *home = getenv("HOME");
    if (!home) {
        perror("Unable to get the home directory");
        return;
    }

    // Construct the path to smain under the home directory
    snprintf(cwd, sizeof(cwd), "%s/smain", home);
    // Check if the file type is .c
    if (strcmp(filetype, ".c") == 0) {
        // Handle .c file type by creating a tar archive and sending it directly to the client
        // Create a pipe to connect the tar output to the socket
        if (pipe(pipefd) == -1) {
            perror("pipe error");
            return;
        }

        // Fork a process to run the tar command
        pid = fork();
        if (pid == -1) {
            perror("fork error");
            return;
        } else if (pid == 0) {
            // Child process: execute the tar command
            close(pipefd[0]);  // Close the read end of the pipe

            // Redirect the tar output to the write end of the pipe
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            // Create tar of .c files in smain directory and output it to stdout
            snprintf(command, BUF_SIZE, "cd %s && tar -cvf - $(find . -type f -name '*.c')", cwd);

            // Execute the tar command
            execl("/bin/sh", "sh", "-c", command, (char *)NULL);

            // If execl fails
            perror("execl error");
            exit(EXIT_FAILURE);
        } else {
            // Parent process: read the tar output from the pipe and send it over the socket
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
            wait(NULL);  // Wait for the child process to finish
        }

        // Properly shut down the connection after sending all data
        shutdown(client_sock, SHUT_WR);
        printf("Tar file sent to client.\n");

    } 
    // Check if the file type is .pdf
    else if (strcmp(filetype, ".pdf") == 0) {
        // Handle .pdf file type by requesting the tar archive from Spdf server and sending it to the client
        requestFileFromServer("pdf.tar", "127.0.0.1", 9801, client_sock);
        printf("The .pdf tar file received from the Spdf server has been forwarded to the client.\n");

    } 
    // Check if the file type is .txt
    else if (strcmp(filetype, ".txt") == 0) {
        // Handle .txt file type by requesting the tar archive from Stext server and sending it to the client
        requestFileFromServer("text.tar", "127.0.0.1", 9800, client_sock);
        printf("The .txt tar file received from the Stext server has been forwarded to the client.\n");

    } else {
        printf("Unsupported file type\n");
    }
}

// Function to request a list of files from servers
void requestFileListFromServer(const char *server_ip, int server_port, const char *command, const char *subdir, char *file_list) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    ssize_t n;

    // Create a socket to connect to servers
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }
    // Set up the address structure for the servers
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }
    // Connect to the servers
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Send the display command with the directory path to the servers
    snprintf(buffer, BUF_SIZE, "%s %s", command, subdir);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the file list from the servers and append it to file_list
    while ((n = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        buffer[n] = '\0';  // Ensure null-terminated string
        strcat(file_list, buffer);
    }

    close(sock);
}

// Function to collect files of a specific type from a directory
void collectFiles(const char *directory, const char *filetype, char *output) {
    char cmd[BUF_SIZE];
    FILE *fp;

    // Construct the find command to find files of the specified type in the directory
    snprintf(cmd, sizeof(cmd), "find %s -type f -name '*%s'", directory, filetype); 

    // Execute the command and open a pipe to read its output
    fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        return;
    }
    // Read the output of the command and append it to the output buffer
    while (fgets(cmd, sizeof(cmd), fp) != NULL) {
        strcat(output, cmd);
    }

    pclose(fp);
}

void displayCommandExecution(const char *pathname, int client_sock) {
    char c_files[BUF_SIZE * 10] = {0};  // Buffer for .c files
    char pdf_files[BUF_SIZE * 10] = {0};  // Buffer for .pdf files
    char txt_files[BUF_SIZE * 10] = {0};  // Buffer for .txt files
    char combined_files[BUF_SIZE * 30] = {0};  // Combined buffer
    char spdf_path[BUF_SIZE];
    char stext_path[BUF_SIZE];

    // Collect .c files from the provided directory
    collectFiles(pathname, ".c", c_files);

    // Copy the original path to spdf_path and handle both "/smain" and "/smain/"
    strncpy(spdf_path, pathname, sizeof(spdf_path));
    spdf_path[sizeof(spdf_path) - 1] = '\0'; // Ensure null termination
    char *replace_spdf = strstr(spdf_path, "/smain");
    if (replace_spdf) {
        // Check if the path ends with "/smain" and not "/smain/"
        if (replace_spdf[strlen(replace_spdf) - 1] == '/') {
            snprintf(replace_spdf, BUF_SIZE - (replace_spdf - spdf_path), "/spdf/%s", replace_spdf + strlen("/smain/"));
        } else {
            snprintf(replace_spdf, BUF_SIZE - (replace_spdf - spdf_path), "/spdf%s", replace_spdf + strlen("/smain"));
        }
    }

    // Debug: Print the transformed path for Spdf
    printf("Transformed path to send to Spdf: %s\n", spdf_path);

    // Collect .pdf files from Spdf directory
    requestFileListFromServer("127.0.0.1", 9801, "display", spdf_path, pdf_files);

    // Copy the original path to stext_path and handle both "/smain" and "/smain/"
    strncpy(stext_path, pathname, sizeof(stext_path));
    stext_path[sizeof(stext_path) - 1] = '\0'; // Ensure null termination
    char *replace_stext = strstr(stext_path, "/smain");
    if (replace_stext) {
        // Check if the path ends with "/smain" and not "/smain/"
        if (replace_stext[strlen(replace_stext) - 1] == '/') {
            snprintf(replace_stext, BUF_SIZE - (replace_stext - stext_path), "/stext/%s", replace_stext + strlen("/smain/"));
        } else {
            snprintf(replace_stext, BUF_SIZE - (replace_stext - stext_path), "/stext%s", replace_stext + strlen("/smain"));
        }
    }

    // Debug: Print the transformed path for Stext
    printf("Transformed path to send to Stext: %s\n", stext_path);

    // Collect .txt files from Stext directory
    requestFileListFromServer("127.0.0.1", 9800, "display", stext_path, txt_files);

    // Combine all lists into one
    strcat(combined_files, c_files);
    strcat(combined_files, pdf_files);
    strcat(combined_files, txt_files);

    // Send the combined list to the client
    send(client_sock, combined_files, strlen(combined_files), 0);

    // Properly close the write side of the socket to signal end of data
    shutdown(client_sock, SHUT_WR);
}

// Function to expand ~ to the user's home directory
void tildePathOperation(char *path, char *expanded_path, size_t size) {
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