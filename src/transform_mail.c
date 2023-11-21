#include "include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

// Renaming the main function to transform_mail
void transform_mail(const char *input_file_path) {
    FILE *input_file = fopen(input_file_path, "r");
    if (!input_file) {
        log_error("Error opening input file %s", input_file_path);
        return;
    }
    log_info("Opened input file %s for transformation", input_file_path);

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, BUFFER_SIZE, input_file)) {
        log_debug("Read line: %s", buffer);
        // Output the original line
        printf("%s", buffer);

        // Check for end of headers (empty line)
        if (strcmp(buffer, "\r\n") == 0) {
            // Append some transformation text after headers
            printf("Transformed: This email has been processed.\r\n");
            log_info("Transformation applied to email in file %s", input_file_path);
        }
    }

    fclose(input_file);
    log_info("Completed processing and closed file %s", input_file_path);
}
