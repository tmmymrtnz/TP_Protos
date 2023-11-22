#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


#define BUFFER_SIZE 1024

char* transform_mail(const char *input_file_path, const char *command) {
    char full_command[BUFFER_SIZE];
    snprintf(full_command, sizeof(full_command), "cat %s | %s", input_file_path, command);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Child process
        close(pipefd[0]); // Close the read end of the pipe

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(pipefd[1]); // Close the write end of the pipe

        execl("/bin/sh", "sh", "-c", full_command, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else {  // Parent process
        close(pipefd[1]); // Close the write end of the pipe

        char *transformed_content = malloc(BUFFER_SIZE);
        if (!transformed_content) {
            perror("malloc");
            close(pipefd[0]);
            exit(EXIT_FAILURE);
        }

        ssize_t total_bytes = 0;
        ssize_t bytes_read;
        ssize_t buffer_capacity = BUFFER_SIZE;

        while ((bytes_read = read(pipefd[0], transformed_content + total_bytes, BUFFER_SIZE)) > 0) {
            total_bytes += bytes_read;

            if (total_bytes + BUFFER_SIZE > buffer_capacity) {
                buffer_capacity += BUFFER_SIZE;
                transformed_content = realloc(transformed_content, buffer_capacity);
                if (!transformed_content) {
                    perror("realloc");
                    close(pipefd[0]);
                    exit(EXIT_FAILURE);
                }
            }
        }

        close(pipefd[0]); // Close the read end of the pipe
        int status;
        waitpid(pid, &status, 0);

        if (bytes_read < 0) {
            perror("read");
            free(transformed_content);
            exit(EXIT_FAILURE);
        }

        transformed_content[total_bytes] = '\0';
        return transformed_content;
    }
}
