#include "auth.h"
#include <stdio.h>
#include <string.h>

#define CREDENTIALS_FILE "credentials.txt"

int authenticate(const char *username, const char *password) {
    // FILE *file = fopen(CREDENTIALS_FILE, "r");
    // if (!file) {
    //     perror("Could not open credentials file");
    //     return 0;
    // }

    // char line[256];
    // while (fgets(line, sizeof(line), file)) {
    //     char stored_username[128], stored_password[128];

    //     sscanf(line, "%127[^:]:%127s", stored_username, stored_password);

    //     if (strcmp(username, stored_username) == 0 && strcmp(password, stored_password) == 0) {
    //         fclose(file);
    //         return 1; // Autenticación exitosa
    //     }
    // }

    // fclose(file);
    // return 0; // Autenticación fallida
    if (strcmp(username, "user") == 0 && strcmp(password, "pass") == 0) {
        return 1;
    } else {
        return 0;
    }
}
