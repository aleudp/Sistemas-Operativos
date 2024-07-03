# Sistema de Archivos Simulado

Este programa simula un sistema de archivos básico similar a UNIX/Linux. A continuación se describen los comandos disponibles:

## Comandos Disponibles

### `mkdir [nombre]`

Crea un nuevo directorio con el nombre especificado.

### `touch [nombre]`

Crea un nuevo archivo vacío con el nombre especificado.

### `cd [nombre]`

Cambia al directorio especificado por `[nombre]`. Utiliza `cd ..` para regresar al directorio padre.

### `ls`

Muestra una lista de archivos y directorios en el directorio actual.

### `ls-l`

Muestra una lista detallada de archivos y directorios en el directorio actual, incluyendo permisos y fecha de creación.

### `ls-li`

Muestra una lista detallada de archivos y directorios en el directorio actual, incluyendo el número de inodo, permisos y fecha de creación.

### `ls-R`

Muestra recursivamente la estructura de directorios, comenzando desde el directorio actual.

### `rm [nombre]`

Elimina el archivo especificado por `[nombre]`.

### `rmdir [nombre]`

Elimina el directorio especificado por `[nombre]`.

### `mv [viejo_nombre] [nuevo_nombre]`

Renombra un archivo o directorio de `[viejo_nombre]` a `[nuevo_nombre]`.

### `chmod [nombre] [permisos]`

Cambia los permisos del archivo o directorio especificado por `[nombre]` utilizando `[permisos]`, donde los permisos se especifican en formato numérico (por ejemplo, 755).

### `find [tipo] [nombre]`

Busca un archivo (`f`) o directorio (`d`) específico con el nombre `[nombre]` en el sistema de archivos.

### `exit`

Cierra el programa del sistema de archivos simulado.

---


