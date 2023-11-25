# TPE Protos

Este proyecto contiene un servidor y un conjunto de herramientas asociadas para la gestión de protocolos de red.

## Estructura del Repositorio

### Ubicación de Materiales

- **Código Fuente:** Todo el código fuente se encuentra dentro de la carpeta `src/` y `admin/`.
- **Pruebas Unitarias:** Las pruebas se encuentran en la carpeta `test/`.

## Generación de Ejecutables

Para compilar el proyecto y generar los ejecutables, sigue estos pasos:

```bash
$ sudo apt install check  # Instalar dependencias necesarias
$ make all                   # Compilar el proyecto con gcc
o
$ make all CC=clang           # Compilar con clang
```

### Ejecutables Generados

Los ejecutables generados se encuentran en:

- **Servidor Principal:** `./pop3d`
- **Cliente para Administrador:** `./bin/admin`

### Ejecución

Para ejecutar el servidor principal:

```bash
$ ./pop3d [opciones]
```

Donde `[opciones]` puede incluir:

- `-p [puerto IPv4]` para especificar el puerto IPv4.
- `-P [puerto admin]` para especificar el puerto admin.
- `-d [directorio de correo]` para establecer el directorio de correos.
- `-u [usuario]:[contraseña]` para crear un usuario y una contraseña.

Para ejecutar el cliente del administrador:

```bash
$ ./bin/admin [contraseña_admin] [puerto] [opciones]
```

Donde `[opciones]` puede incluir:

Donde [OPCIÓN] puede incluir:

- `-h` para obtener ayuda.
- `-A <nombre> <contraseña>` para agregar un usuario al servidor. Requiere nombre de usuario y contraseña.
- `-D <nombre>` para eliminar un usuario del servidor. Requiere nombre de usuario.
- `-R <nombre>` para restablecer la contraseña de un usuario. Requiere nombre de usuario.
- `-C <antigua_contraseña> <nueva_contraseña>` para cambiar la contraseña. Requiere la contraseña antigua y la nueva.
- `-m` para establecer/obtener el número máximo de usuarios. Opcionalmente requiere el recuento máximo de usuarios.
- `-d` para obtener la ruta del directorio de correos.
- `-M <ruta>` para establecer la ruta del directorio de correos. Requiere la ruta del maildir.
- `-a` para obtener el número total de conexiones.
- `-c` para obtener el número actual de conexiones.
- `-b` para obtener el número de bytes transferidos.
- `-s` para obtener el estado del servidor.
- `-u` para listar todos los usuarios.

Nota: Puede ser necesario ejecutar el servidor y el admin con `sudo` para permitir el uso de puertos bien conocidos.

### Usuarios y Credenciales

El servidor incluye usuarios de prueba con las siguientes credenciales:

- **Usuarios:** `user1`, `user2`, `user3`
- **Contraseñas:** `pass1`, `pass2`, `pass3` (correspondientes a cada usuario)

Las credenciales del administrador son:

- **Contraseña:** `default`
