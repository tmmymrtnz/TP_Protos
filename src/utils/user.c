#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/user.h"

void initializeUsers(void) {

    usersStruct = malloc(sizeof(TUsers));
    usersStruct->users = malloc(sizeof(TUser) * MAX_CLIENTS);
    usersStruct->count = 1;
    usersStruct->max_users = MAX_CLIENTS;

    if (usersStruct->users == NULL) {
        perror("Error: could not allocate users array");
        exit(1);
    }

    addUser("default", "default");
    usersStruct->users[0].isAdmin = 1;
}

int addUser(char *username, char *password) {
    if(usersStruct->count == MAX_CLIENTS)
        return 1;
    if(username == NULL || password == NULL)
        return 1;
    if(findUser(username) != - 1)
        return 1;

    //Las credenciales son validas y el usuario aun no existe, lo agrego a la estructura.
    strncpy(usersStruct->users[usersStruct->count].username, username, strlen(username) + 1);
    strncpy(usersStruct->users[usersStruct->count].password, password, strlen(password) + 1);
    usersStruct->users[usersStruct->count].isAdmin = 0;
    usersStruct->users[usersStruct->count].isConnected = 0;
    
    usersStruct->count++;
    return 0; // Exito
}

int deleteUser(char *username) {
    int i;
    if( ( i = findUser(username) ) == - 1)
        return 1;
    
    if(usersStruct->users[i].isAdmin == 1)
        return 1;

    for(int j = i; j < usersStruct->count - 1; j++)
        usersStruct->users[j] = usersStruct->users[j + 1];
    
    usersStruct->count--;
    return 0; // Exito
}

int findUser(char *username) {
    if(username == NULL)
        return - 1;

    for(int i = 0 ; i < usersStruct->count; i++)
        if(strcmp(usersStruct->users[i].username, username) == 0)
            return i;
    
    return - 1;
}

int validateAdminUsername(char *username) {
    int i;
    if( ( i = findUser(username) ) == - 1)
        return 1;

    if(usersStruct->users[i].isAdmin == 1 && usersStruct->users[i].isConnected == 0)
        return 0;
    else
        return 1;
}

int validateAdminCredentials(char *username, char *password) {
    int i;
    if( (i = findUser(username)) == -1 )
        return 1;
    if(strcmp(usersStruct->users[i].username, username) == 0 && strcmp(usersStruct->users[i].password, password) == 0) {
        usersStruct->users[i].isConnected = 1;
        return 0;
    }
    
    return 1;
}

int validateUsername(char *username) {
    if(findUser(username) != - 1)
        return 0;
    else
        return 1;
}

int validateUserCredentials(char *username, char *password) {
    int i;
    if((i = findUser(username)) == - 1)
        return 1;
    else if(strcmp(usersStruct->users[i].password, password) == 0 && usersStruct->users[i].isConnected == 0) {
        usersStruct->users[i].isConnected = 1;
        return 0;
    }

    return 1; //Fallo la autenticacion
}

int changePassword(char *username, char *oldPassword, char *newPassword) {
    if (username == NULL || oldPassword == NULL || newPassword == NULL)
        return 1;

    int i;
    if ((i = findUser(username)) == -1)
        return 1;
    if(strcmp(usersStruct->users[i].password, oldPassword) == 0) {
        strncpy(usersStruct->users[i].password, newPassword, strlen(newPassword) + 1);
        return 0;
    }

    return 1;
}

int resetUserPassword(char *username) {
    int i;
    if((i = findUser(username)) == -1)
        return 1;
    else {
        strncpy(usersStruct->users[i].password, "password", strlen("password") + 1);
        return 0;
    }

    return 1;
}

void freeUsers(void) {
    free(usersStruct->users);
    free(usersStruct);
}
