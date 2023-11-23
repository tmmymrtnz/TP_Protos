# TPE Protos

Este proyecto contiene un servidor y un conjunto de herramientas asociadas para la gestión de protocolos de red.

## Estructura del Repositorio

### Ubicación de Materiales

- **Código Fuente:** Todo el código fuente se encuentra dentro de la carpeta `src/`.
- **Pruebas Unitarias:** Las pruebas se encuentran en la carpeta `test/`.

## Generación de Ejecutables

Para compilar el proyecto y generar los ejecutables, sigue estos pasos:

```bash
$ sudo apt install check  # Instalar dependencias necesarias
$ make                    # Compilar el proyecto con gcc
o
$ make CC=clang           #Compilar con clang
```

### Ejecutables Generados

Los ejecutables generados se encuentran en:

- **Servidor Principal:** `./pop3d`

### Ejecución

Para ejecutar el servidor principal:

```bash
$ ./pop3d [opciones]
```

Donde `[opciones]` puede incluir:

- `-p [puerto IPv4]` para especificar el puerto IPv4.
- `-P [puerto IPv6]` para especificar el puerto IPv6.
- `-d [directorio de correo]` para establecer el directorio de correos.
- `-t [comando de transformación]` para definir un comando de transformación.

Nota: Puede ser necesario ejecutar el servidor con `sudo` para permitir el uso de puertos bien conocidos.


