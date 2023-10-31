#include <stddef.h>
#define MAX_CLIENTS 500

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
} TUsers;

TUsers * usersStruct;

int addUser(char *username, char* password);

//Elimina a un usuario.
int deleteUser(char *username);

//Devuelve el indice del vector donde esta alojado el usuario.
int findUser(char *username);

//Devuelve 0 si el usuario es admin, 1 en otro caso.
int validateAdminUsername(char *username);

//Autentica a un administrador y lo marca online.
int validateAdminCredentials(char *username, char *password);

//Devuelve 0 si encontro al usuario, 1 en otro caso.
int validateUsername(char *username);

//Devuelve 0 si autentico al usuario, 1 en otro caso.
int validateUserCredentials(char *username, char *password);

//Devuelve 0 si cambio la contra, 1 en otro caso.
int changePassword(char *username, char *oldPassword, char *newPassword);

//Devuelve 0 si reseteo la contra a "password", 1 en otro caso
int resetUserPassword(char *username);

//Libera toda la memoria utilizada por las estructuras de usuarios.
void freeUsers();