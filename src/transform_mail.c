#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

char* transform_mail(const char *input_file_path) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Child process
        close(pipefd[0]); // Close the read end of the pipe

        // Redirect the standard output to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]); // Close the write end of the pipe

        // Execute command
        execlp("wc", "wc", input_file_path, NULL);
        perror("exec");
        exit(EXIT_FAILURE);
    } else {  // Parent process
        close(pipefd[1]); // Close the write end of the pipe

        // Get the file size
        FILE* file = fopen(input_file_path, "r");
        if (!file) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fclose(file);

        // Allocate memory for the content
        char *buffer = malloc((file_size + 1) * sizeof(char));  // Add space for null-terminator
        if (!buffer) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Read from the pipe
        ssize_t total_bytes = 0;
        ssize_t bytes_read;

        while ((bytes_read = read(pipefd[0], buffer + total_bytes, file_size - total_bytes)) > 0) {
            total_bytes += bytes_read;
        }

        close(pipefd[0]); // Close the read end of the pipe
        wait(NULL); // Wait for the child process to finish

        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        buffer[total_bytes] = '\0';  // Null-terminate the string
        return buffer;
    }
}
