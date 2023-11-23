#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024*1024*1024

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
    FILE *file = fopen(input_file_path, "r");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    if (fread(file_content, 1, file_size, file) != file_size) {
        perror("fread");
        fclose(file);
        free(file_content);
        return NULL;
    }

    fclose(file);
    file_content[file_size] = '\0';

    char *content_without_end = remove_end_of_response(file_content);
    free(file_content);

    if (!content_without_end) {
        perror("remove_end_of_response");
        return NULL;
    }

    char full_command[BUFFER_SIZE];
    snprintf(full_command, sizeof(full_command), "echo \"%s\" | %s", content_without_end, command);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        free(content_without_end);
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free(content_without_end);
        return NULL;
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close the read end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(pipefd[1]); // Close the write end of the pipe

        execl("/bin/sh", "sh", "-c", full_command, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else { // Parent process
        close(pipefd[1]); // Close the write end of the pipe

        char *transformed_content = NULL;
        size_t total_bytes = 0;
        size_t buffer_size = BUFFER_SIZE;

        while (1) {
            transformed_content = realloc(transformed_content, total_bytes + buffer_size + 1);
            if (!transformed_content) {
                perror("realloc");
                close(pipefd[0]);
                free(content_without_end);
                return NULL;
            }

            ssize_t bytes_read = read(pipefd[0], transformed_content + total_bytes, buffer_size);
            if (bytes_read <= 0) {
                break;
            }
            total_bytes += bytes_read;
        }

        close(pipefd[0]); // Close the read end of the pipe
        free(content_without_end);

        transformed_content[total_bytes] = '\0';

        return transformed_content;
    }
}