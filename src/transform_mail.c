#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024*1024

char* remove_end_of_response(const char* email_content) {
    size_t len = strlen(email_content);
    char* result = malloc(len + 1);
    if (!result) {
        perror("malloc");
        return NULL;
    }

    size_t read_index = 0, write_index = 0;

    while (read_index < len) {
        if (read_index + 5 <= len &&
            email_content[read_index] == '\r' && email_content[read_index + 1] == '\n' &&
            email_content[read_index + 2] == '.' && email_content[read_index + 3] == '\r' &&
            email_content[read_index + 4] == '\n') {
            read_index += 5;  // Skip the '\r\n.\r\n' sequence
        } else {
            result[write_index++] = email_content[read_index++];
        }
    }
    result[write_index] = '\0';

    return result;
}

char* transform_mail(const char *input_file_path, const char *command) {
    int pipefd_in[2], pipefd_out[2];
    if (pipe(pipefd_in) == -1 || pipe(pipefd_out) == -1) {
        perror("pipe");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd_in[0]);
        close(pipefd_in[1]);
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        return NULL;
    }

    if (pid == 0) { // Child process
        close(pipefd_in[1]);  // Close the write end of input pipe
        close(pipefd_out[0]); // Close the read end of output pipe

        // Redirect stdin and stdout
        dup2(pipefd_in[0], STDIN_FILENO);
        dup2(pipefd_out[1], STDOUT_FILENO);

        // Execute the command
        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd_in[0]);  // Close the read end of input pipe
    close(pipefd_out[1]); // Close the write end of output pipe

    FILE *input_file = fopen(input_file_path, "r");
    if (!input_file) {
        perror("fopen");
        close(pipefd_in[1]);
        close(pipefd_out[0]);
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    size_t total_bytes = 0;
    char *transformed_content = NULL;

    // Read from file and write to child's stdin
    while (fgets(buffer, BUFFER_SIZE, input_file) != NULL) {
        write(pipefd_in[1], buffer, strlen(buffer));
    }
    fclose(input_file);
    close(pipefd_in[1]); // Close input pipe after writing

    // Read from child's stdout
    while (1) {
        ssize_t bytes_read = read(pipefd_out[0], buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) break;

        char *temp = realloc(transformed_content, total_bytes + bytes_read + 1);
        if (!temp) {
            perror("realloc");
            free(transformed_content);
            close(pipefd_out[0]);
            return NULL;
        }
        transformed_content = temp;
        memcpy(transformed_content + total_bytes, buffer, bytes_read);
        total_bytes += bytes_read;
        transformed_content[total_bytes] = '\0'; // Null-terminate
    }

    close(pipefd_out[0]); // Close output pipe after reading
    waitpid(pid, NULL, 0); // Wait for child process to finish

    return transformed_content;
}
