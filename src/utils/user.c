#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/user.h"
#include "../include/logger.h"

static TUsers *usersStruct;

TUsers *getUsersStruct(void) {
    return usersStruct;
}

void initialize_users(void) {
    usersStruct = malloc(sizeof(TUsers));

    if (usersStruct == NULL) {
        perror("Error: could not allocate users structure");
        exit(1);
    }

    usersStruct->users = malloc(sizeof(TUser) * MAX_CLIENTS);
    if (usersStruct->users == NULL) {
        perror("Error: could not allocate users array");
        free(usersStruct);
        exit(1);
    }

    usersStruct->count = 0;
    usersStruct->max_users = MAX_CLIENTS;

    // Directly add the default user
    strncpy(usersStruct->users[0].username, "default", sizeof(usersStruct->users[0].username));
    strncpy(usersStruct->users[0].password, "default", sizeof(usersStruct->users[0].password));
    usersStruct->users[0].isAdmin = 1;
    usersStruct->users[0].isConnected = 0;
    usersStruct->count++;

    // Add other users
    add_user("user1", "pass1");
    add_user("user2", "pass2");
    add_user("user3", "pass3");
}


int add_user(const char *username, const char* password) {
    if(usersStruct->count == usersStruct->max_users)
        return 1;
    if(username == NULL || password == NULL)
        return 1;
    if(find_user(username) != - 1)
        return 1;

    //Las credenciales son validas y el usuario aun no existe, lo agrego a la estructura.
    strncpy(usersStruct->users[usersStruct->count].username, username, strlen(username) + 1);
    strncpy(usersStruct->users[usersStruct->count].password, password, strlen(password) + 1);
    usersStruct->users[usersStruct->count].isAdmin = 0;
    usersStruct->users[usersStruct->count].isConnected = 0;
    
    usersStruct->count++;
    return 0; // Exito
}

int delete_user(const char *username) {
    int i;
    if( ( i = find_user(username) ) == - 1)
        return 1;
    
    if(usersStruct->users[i].isAdmin == 1)
        return 1;

    for(int j = i; j < usersStruct->count - 1; j++)
        usersStruct->users[j] = usersStruct->users[j + 1];
    
    usersStruct->count--;
    return 0; // Exito
}

int find_user(const char *username) {
    if(username == NULL)
        return - 1;

    for(int i = 0 ; i < usersStruct->count; i++)
        if(strcmp(usersStruct->users[i].username, username) == 0)
            return i;
    
    return - 1;
}

int validate_admin_username(const char *username) {
    int i;
    if( ( i = find_user(username) ) == - 1)
        return 1;

    if(usersStruct->users[i].isAdmin == 1 && usersStruct->users[i].isConnected == 0)
        return 0;
    else
        return 1;
}

int validate_admin_credentials(const char *username, const char *password) {
    int i;
    if( (i = find_user(username)) == -1 )
        return 1;
    if(strcmp(usersStruct->users[i].username, username) == 0 && strcmp(usersStruct->users[i].password, password) == 0) {
        usersStruct->users[i].isConnected = 1;
        return 0;
    }
    
    return 1;
}

int validate_username(const char *username) {
    if(find_user(username) != - 1)
        return 0;
    else
        return 1;
}

int validate_user_credentials(const char *username, const char *password) {
    int i;
    if((i = find_user(username)) == - 1)
        return 1;
    else if(strcmp(usersStruct->users[i].password, password) == 0 && usersStruct->users[i].isConnected == 0) {
        usersStruct->users[i].isConnected = 1;
        return 0;
    }

    return 1; //Fallo la autenticacion
}

int change_password(const char *username, const char *oldPassword, const char *newPassword) {
    if (username == NULL || oldPassword == NULL || newPassword == NULL)
        return 1;

    int i;
    if ((i = find_user(username)) == -1)
        return 1;
    if(strcmp(usersStruct->users[i].password, oldPassword) == 0) {
        strncpy(usersStruct->users[i].password, newPassword, strlen(newPassword) + 1);
        return 0;
    }

    return 1;
}

int reset_user_password(const char *username) {
    int i;
    if((i = find_user(username)) == -1)
        return 1;
    else {
        strncpy(usersStruct->users[i].password, "password", strlen("password") + 1);
        return 0;
    }

    return 1;
}

int reset_user_connection(const char *username) {
    int userIndex = find_user(username);
    if (userIndex == -1) {
        // User not found
        return 1;
    }

    usersStruct->users[userIndex].isConnected = 0;
    return 0; // Success
}

int is_admin(const char *username) {
    int userIndex = find_user(username);
    if (userIndex == -1) {
        // User not found
        return 0;
    }

    return usersStruct->users[userIndex].isAdmin;
}

void freeUsers(void) {
    free(usersStruct->users);
    free(usersStruct);
}
