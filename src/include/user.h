#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define MAX_CLIENTS 1000

#define USERNAME_MAX_LENGTH 256
#define PASSWORD_MAX_LENGTH 256

typedef struct {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    int isAdmin;
    int isConnected;
} TUser;

typedef struct {
    TUser * users;
    int count;
    int max_users;
    int isTransformed;
} TUsers;

TUsers *getUsersStruct();

//Inicializa la estructura y agrega usuarios default.
void initialize_users(void);

int add_user(const char *username, const char* password);

//Elimina a un usuario.
int delete_user(const char *username);

//Devuelve el indice del vector donde esta alojado el usuario.
int find_user(const char *username);

//Devuelve 0 si el usuario es admin, 1 en otro caso.
int validate_admin_username(const char *username);

//Autentica a un administrador y lo marca online.
int validate_admin_credentials(const char *username, const char *password);

//Devuelve 0 si encontro al usuario, 1 en otro caso.
int validate_username(const char *username);

//Devuelve 0 si autentico al usuario, 1 en otro caso.
int validate_user_credentials(const char *username, const char *password);

//Devuelve 0 si cambio la contra, 1 en otro caso.
int change_password(const char *username, const char *oldPassword, const char *newPassword);

//Devuelve 0 si reseteo la contra a "password", 1 en otro caso
int reset_user_password(const char *username);

int reset_user_connection(const char *username);

int is_admin(const char *username);

//Libera toda la memoria utilizada por las estructuras de usuarios.
void freeUsers(void);
