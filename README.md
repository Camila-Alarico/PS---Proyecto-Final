# ADMIN en Linux

**Herramienta de administracion de sistemas para Linux, escrita en C.**
Proyecto Final - Curso de *Programacion de Sistemas* - UNSA 2026.

`ADMIN en Linux` es un programa de terminal con **menu interactivo a colores** que integra
seis modulos de administracion en un solo binario, aplicando llamadas al sistema (syscalls),
seniales, procesos, E/S de bajo nivel, sockets de red y programacion concurrente con hilos.

---

## Modulos

| # | Modulo | Que hace | Conceptos del silabo |
|---|--------|----------|----------------------|
| 1 | **Administrador de Tareas** | Lista procesos con CPU%/memoria, busca por nombre, termina/suspende/reanuda, arbol de procesos | Procesos, seniales, `/proc`, control de flujo excepcional |
| 2 | **Shell de Archivos** | Listar (`ls`), copiar, mover, borrar, buscar y estadisticas | E/S a nivel de sistema (`open/read/write`), metadata (`stat`) |
| 3 | **Comandos Linux** | Ejecuta comandos capturando stdout/stderr por separado + historial | `fork/execvp/pipe/dup2`, E/S multiplexada (`poll`) |
| 4 | **Backups Incrementales** | Snapshots incrementales por hash, restauracion verificada, compresion gzip | Representacion de datos, E/S, `zlib` |
| 5 | **Analizador de Scripts Bash** | Detecta variables, ciclos (for/while/until) y condicionales | Programacion en bash (Tema 02) |
| 6 | **Gestor de Descargas** | Descargas HTTP concurrentes por sockets con progreso e integridad SHA-256 | Redes/sockets (Tema 14), concurrencia con hilos (Tema 15) |

+ **Utilidad extra:** checksum SHA-256 de cualquier archivo (implementacion propia).

---

## Compilacion y ejecucion (Linux)

Requisitos: `gcc`, `make`, `zlib` (`zlib1g-dev`), `pthread` (incluida en glibc).

```bash
# En Debian/Ubuntu, si falta zlib:
sudo apt-get install build-essential zlib1g-dev

# Compilar (compilacion separada, genera ./admin)
make

# Ejecutar
./admin
# o directamente:
make run

# Limpiar objetos y binario
make clean
```

---

## Estructura del proyecto

```
admin-linux/
|-- Makefile                 # compilacion separada (.c -> obj/.o -> admin)
|-- README.md
|-- include/                 # cabeceras (.h) de cada modulo
|-- src/                     # implementacion (.c) de cada modulo
|   |-- main.c               # menu principal (TUI)
|   |-- ui.c                 # colores, menus, barras de progreso
|   |-- util.c               # entrada segura, SHA-256, tamanos, copia I/O
|   |-- procesos.c           # administrador de tareas (/proc + seniales)
|   |-- archivos.c           # shell de archivos
|   |-- comandos.c           # ejecucion de comandos + historial
|   |-- backup.c             # backups incrementales + restore + gzip
|   |-- analizador.c         # analizador de scripts bash
|   `-- descargas.c          # gestor de descargas (sockets + hilos)
|-- samples/                 # datos de ejemplo para la demo
|   |-- ejemplo.sh           # script bash para el analizador
|   `-- datos/               # carpeta para probar backups
|-- docs/
|   |-- documentacion_tecnica.md
|   |-- manual_usuario.md
|   `-- presentacion.md
`-- demo.sh                  # guion de demostracion rapida
```

---

## Demo rapida

```bash
make
./demo.sh          # prepara datos y un servidor HTTP local de prueba
./admin            # explora los menus
```

Dentro del programa, prueba:
- Menu 5 -> analiza `samples/ejemplo.sh`
- Menu 4 -> respalda `samples/datos`, luego restaura
- Menu 6 -> agrega `http://127.0.0.1:8099/archivo.bin` y descarga

---

## Autores

Equipo del curso de Programacion de Sistemas - UNSA 2026.

## Licencia

Proyecto academico de uso educativo.
