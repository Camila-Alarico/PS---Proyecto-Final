# ZETCH

ZETCH es un programa de terminal con menú interactivo a colores (TUI) que integra seis módulos de administración en un solo binario, implementando cada funcionalidad mediante llamadas al sistema (syscalls) como procesos, señales, entrada/salida de bajo nivel, sockets de red y programación concurrente con hilos.

## Tabla de contenidos

1. [Arquitectura](#arquitectura)
2. [Estructura de directorios](#estructura-de-directorios)
3. [Requisitos](#requisitos)
4. [Compilación y ejecución](#compilación-y-ejecución)
5. [Módulos](#módulos)
6. [Implementación técnica relevante](#implementación-técnica-relevante)
7. [Manejo de errores](#manejo-de-errores)
8. [Limitaciones conocidas](#limitaciones-conocidas)
9. [Trabajo futuro](#trabajo-futuro)

---

## Arquitectura

El sistema sigue una arquitectura en capas simple:

```
                ┌─────────────┐
                │   main.c    │   Punto de entrada / menú principal (TUI)
                └──────┬──────┘
                       │ despacha según opción elegida
     ┌───────┬─────────┼─────────┬─────────┬──────────┐
     ▼       ▼         ▼         ▼         ▼          ▼
procesos.c archivos.c comandos.c backup.c analizador.c descargas.c
     │       │         │         │         │          │
     └───────┴─────────┴────┬────┴─────────┴──────────┘
                             ▼
                  ┌────────────────────┐
                  │   ui.c / util.c    │  Capa de soporte transversal
                  └────────────────────┘
```

- **`main.c`** muestra el menú principal y delega el control al módulo correspondiente según la opción seleccionada por el usuario.
- Los módulos de dominio (`procesos.c`, `archivos.c`, `comandos.c`, `backup.c`, `analizador.c`, `descargas.c`) son independientes entre sí; ninguno invoca directamente a otro.
- Todos los módulos comparten dos componentes de soporte:
  - **`ui.c`**: capa de presentación (colores ANSI, encabezados de menú, barras de progreso, pausas).
  - **`util.c`**: utilidades comunes (lectura segura de entrada del usuario, cálculo de SHA-256, formateo de tamaños, copia de E/S).

Esta separación permite que cada módulo se compile y pruebe de forma independiente, y que la lógica de presentación no esté mezclada con la lógica de cada funcionalidad.

## Estructura de directorios

Compilación separada: cabeceras (`.h`) en `include/`, implementación (`.c`) en `src/`.

```
admin-linux/
├── Makefile              # compilación separada (.c -> obj/.o -> admin)
├── README.md
├── include/               # cabeceras (.h) de cada módulo
│   ├── analizador.h
│   ├── archivos.h
│   ├── backup.h
│   ├── comandos.h
│   ├── descargas.h
│   ├── procesos.h
│   ├── ui.h
│   └── util.h
├── src/                   # implementación (.c) de cada módulo
│   ├── main.c             # menú principal (TUI)
│   ├── ui.c                # colores, menús, barras de progreso
│   ├── util.c              # entrada segura, SHA-256, tamaños, copia I/O
│   ├── procesos.c          # administrador de tareas (/proc + señales)
│   ├── archivos.c          # shell de archivos
│   ├── comandos.c          # ejecución de comandos + historial
│   ├── backup.c            # backups incrementales + restore + gzip
│   ├── analizador.c        # analizador de scripts bash
│   └── descargas.c         # gestor de descargas (sockets + hilos)
├── samples/               # datos de ejemplo para la demo
│   ├── ejemplo.sh          # script bash para el analizador
│   └── datos/              # carpeta para probar backups
├── backups/                # almacén de snapshots generados
├── descargas/               # archivos descargados por el gestor
└── demo.sh                 # guion de demostración rápida
```

## Requisitos

| Dependencia | Uso |
|---|---|
| `gcc` | Compilador |
| `make` | Automatización de compilación |
| `zlib` (`zlib1g-dev`) | Compresión gzip de los backups incrementales |
| `pthread` (incluida en glibc) | Concurrencia del gestor de descargas |

Instalación en Debian/Ubuntu:

```bash
sudo apt-get install build-essential zlib1g-dev
```

## Compilación y ejecución

```bash
make          # compila (genera ./admin)
./admin       # ejecuta el programa
make run      # compila y ejecuta en un solo paso
make clean    # limpia objetos y binario
```

Demo rápida con datos de ejemplo:

```bash
make
./demo.sh     # prepara datos y un servidor HTTP local de prueba
./admin       # explora los menús
```

Dentro del programa se recomienda probar:

- Menú 5 → analizar `samples/ejemplo.sh`
- Menú 4 → respaldar `samples/datos`, luego restaurar
- Menú 6 → agregar `http://127.0.0.1:8099/archivo.bin` y descargar

## Módulos

### 1. Administrador de Tareas (`procesos.c`)

Lista los procesos del sistema leyendo directamente el pseudo-sistema de archivos /proc, mostrando PID, PPID, estado, porcentaje de CPU, memoria y nombre. Permite:

- Buscar procesos por nombre.
- Finalizar un proceso (SIGTERM) o forzar su terminación (SIGKILL).
- Suspender (SIGSTOP) y reanudar (SIGCONT) un proceso.
- Visualizar el árbol de procesos, construido a partir de la relación PID/PPID leída de /proc.

### 2. Shell de Archivos (archivos.c)

Reimplementa operaciones básicas de shell (`ls`, `cp`, `mv`, `rm`, búsqueda recursiva, estadísticas) usando syscalls de bajo nivel: `open()`, `read()`, `write()` para copiar contenido, y `stat()` para obtener metadata (permisos, tamaño, fecha de modificación) sin depender de utilidades externas.

### 3. Comandos Linux (comandos.c)

Ejecuta comandos arbitrarios del sistema capturando `stdout` y `stderr` **por separado**, además de mantener un historial persistente.

Flujo de ejecución:

1. La línea ingresada se tokeniza con strtok() en un arreglo argv[] compatible con execvp().
2. Se crean dos pipes (pipe()), uno para stdout y otro para stderr.
3. Se hace fork(). En el hijo, se redirigen ambos descriptores estándar a los pipes con dup2() y se ejecuta el comando con execvp().
4. En el padre, se usa poll() para monitorear ambos extremos de lectura simultáneamente sin bloquearse esperando a uno mientras el otro tiene datos pendientes (evita interbloqueo si el hijo escribe grandes volúmenes de datos en ambos flujos a la vez).
5. Se espera al hijo con waitpid() y se reporta el código de salida o la señal que lo terminó.
6. El comando se agrega al historial en memoria (hasta 100 entradas) y se persiste en ~/.admin_history.

### 4. Backups Incrementales (backup.c)

Genera snapshots de una carpeta calculando el hash de cada archivo; solo copia los archivos cuyo hash cambió respecto al snapshot anterior, comprimiendo los datos nuevos con gzip/zlib. La restauración recalcula el SHA-256 del archivo restaurado y lo compara contra el original, marcando corrupción si no coinciden.

### 5. Analizador de Scripts Bash (analizador.c)

Analiza estáticamente un archivo .sh, detectando:

- Declaración y uso de variables.
- Ciclos: for, while, until.
- Condicionales: if, case.
- Definición de funciones.

### 6. Gestor de Descargas (`descargas.c`)

Cliente HTTP implementado directamente sobre sockets TCP (sin librerías HTTP externas), que permite encolar varias URLs y descargarlas de forma concurrente usando un hilo (pthread) por descarga. Cada hilo actualiza su propio estado de progreso, visible desde un monitor en vivo, y al finalizar verifica la integridad del archivo descargado calculando su SHA-256.

## Implementación técnica relevante

**Syscalls y APIs principales utilizadas por módulo:**

| Módulo | Syscalls / APIs |
|---|---|
| Procesos | lectura de /proc/[pid]/stat, /proc/[pid]/status; kill() para señales |
| Archivos | open, read, write, close, stat, opendir/readdir |
| Comandos | fork, execvp, pipe, dup2, poll, waitpid |
| Backups | open/read/write, zlib (deflate/inflate), SHA-256 propio |
| Descargas | socket, connect, send/recv, pthread_create/pthread_join |

## Manejo de errores

- **Comandos:** si execvp() falla (comando inexistente), el proceso hijo reporta el error por stderr y termina con código 127, que el padre muestra al usuario.
- **Creación de procesos/pipes:** fallos de pipe() o fork() se reportan con strerror(errno) sin intentar continuar la ejecución.
- **Archivos:** operaciones sobre rutas inexistentes o sin permisos devuelven el error del sistema en vez de fallar silenciosamente.
- **Backups:** la restauración se valida contra el checksum SHA-256 original antes de darse por exitosa.
- **Descargas:** errores de conexión (host no disponible, URL inválida) se reflejan en el estado de esa descarga puntual, sin detener las demás descargas en curso.

## Limitaciones conocidas

- El gestor de descargas soporta únicamente el protocolo `http://` (no incluye HTTPS/TLS).

## Trabajo futuro

- Soporte HTTPS mediante OpenSSL.
- Programación automática de backups (por ejemplo, vía `cron`).
- Exportación de reportes en formato CSV.

---

*Documentación técnica de ZETCH en Linux — Programación de Sistemas, UNSA 2026.*
