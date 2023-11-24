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

    if(strcmp(command,"cat")==0){ // If the command is cat, we don't need to fork
         return content_without_end;
     }

   if (!content_without_end) {
    perror("remove_end_of_response");
    return NULL;
}

const size_t chunk_size = BUFFER_SIZE - 1;

size_t offset = 0;
char *transformed_content = NULL;

while (offset < strlen(content_without_end)) {
    char *chunk = malloc(chunk_size + 1);
    if (!chunk) {
        perror("malloc");
        free(content_without_end);
        return NULL;
    }

    // Copy a chunk of the content to process
    strncpy(chunk, content_without_end + offset, chunk_size);
    chunk[chunk_size] = '\0';

    // Create a command for the chunk
    char *chunk_command = malloc(strlen(command) + chunk_size + 11);
    if (!chunk_command) {
        perror("malloc");
        free(content_without_end);
        free(chunk);
        return NULL;
    }
    snprintf(chunk_command, strlen(command) + chunk_size + 11, "echo \"%s\" | %s", chunk, command);

    // Fork and execute the command
 

   pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free(content_without_end);
        free(chunk);
        free(chunk_command);
        return NULL;
    }

    if (pid == 0) { // Child process
        char *args[] = { "sh", "-c", chunk_command, NULL };
        execvp(args[0], args);
        exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to finish

        // Read the output from the executed command (if needed)
        // Add your code to read the output if required

        // Append the processed chunk to the transformed content
        char *temp = realloc(transformed_content, offset + chunk_size + 1);
        if (!temp) {
            perror("realloc");
            free(content_without_end);
            free(chunk);
            free(chunk_command);
            free(transformed_content);
            return NULL;
        }
        transformed_content = temp;
        strncpy(transformed_content + offset, chunk, chunk_size);
        transformed_content[offset + chunk_size] = '\0';

        free(chunk);
        free(chunk_command);
    }

    offset += chunk_size;
}

free(content_without_end);

return transformed_content;
}