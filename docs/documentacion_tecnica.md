# Documentacion Tecnica - ADMIN en Linux

**Curso:** Programacion de Sistemas - UNSA 2026
**Lenguaje:** C (estandar gnu11)
**Plataforma:** Linux (probado en Debian 12, kernel 6.x)

---

## 1. Vision general y arquitectura

`ADMIN en Linux` sigue una **arquitectura modular** donde cada funcionalidad principal
es un modulo independiente con su propia cabecera (`.h`) e implementacion (`.c`). El
programa principal (`main.c`) solo orquesta el menu y delega en cada modulo. Esto
facilita la compilacion separada, las pruebas y el mantenimiento.

```
              +------------------+
              |     main.c       |   Menu principal (TUI)
              +--------+---------+
                       | invoca *_menu()
   +---------+---------+---------+---------+-----------+-----------+
   |         |         |         |         |           |           |
procesos  archivos  comandos  backup   analizador  descargas   (utilidad)
   \_________\_________\_________\_________\___________\___________/
                       |
              +--------+---------+
              |  ui.c + util.c   |  Servicios comunes (colores, entrada,
              +------------------+  SHA-256, copia de archivos, formato)
```

### Capas
- **Capa de presentacion (`ui.c`):** colores ANSI, encabezados, barras de progreso,
  mensajes con formato (ok/error/info/advertencia).
- **Capa de utilidades (`util.c`):** lectura segura de entrada, formateo de tamanos,
  permisos estilo `ls`, **SHA-256 propio**, copia de archivos con I/O de bajo nivel,
  creacion recursiva de directorios (`mkdir -p`).
- **Capa de modulos:** cada modulo implementa su logica y su submenu.

---

## 2. Compilacion separada (Makefile)

Cada fuente `src/X.c` se compila a `obj/X.o` de forma independiente y luego se enlazan
todos en el binario `admin`. Se usa detección automatica de fuentes con `wildcard`.

```make
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,obj/%.o,$(SRCS))
admin: $(OBJS)
	gcc -pthread -o admin $(OBJS) -lz
obj/%.o: src/%.c $(DEPS) | obj
	gcc -std=gnu11 -Wall -Wextra -O2 -Iinclude -pthread -c $< -o $@
```

Bibliotecas enlazadas: `-lz` (zlib para compresion) y `-pthread` (hilos POSIX).

---

## 3. Modulo Administrador de Tareas (`procesos.c`)

**Fuente de datos:** el sistema de archivos virtual `/proc`.

- **Listado:** se recorre `/proc`, tomando cada subdirectorio cuyo nombre es numerico
  (un PID). De cada uno se leen:
  - `/proc/<pid>/stat`  -> nombre, estado, PPID, `utime`+`stime` (tiempo de CPU en jiffies).
  - `/proc/<pid>/status` -> `VmRSS` (memoria residente).
  - `/proc/<pid>/cmdline` -> linea de comandos.
- **Calculo de %CPU:** se toman **dos muestras** de los jiffies de cada proceso y del
  total de CPU (`/proc/stat`) separadas por 250 ms. El porcentaje es:
  `%CPU = (delta_proceso / delta_total) * 100 * num_nucleos`.
- **%Memoria:** `VmRSS / MemTotal * 100` (MemTotal de `/proc/meminfo`).
- **Seniales (control de procesos):** `kill(pid, sig)` con:
  - `SIGTERM` (terminar), `SIGKILL` (forzar), `SIGSTOP` (suspender), `SIGCONT` (reanudar).
- **Arbol de procesos:** se construye a partir de la relacion PID/PPID y se imprime
  recursivamente con indentacion.

**Parseo robusto de `/proc/<pid>/stat`:** el nombre del proceso puede contener espacios
y parentesis, por lo que se localiza el ultimo `)` con `strrchr` antes de parsear el
resto de los campos.

---

## 4. Modulo Shell de Archivos (`archivos.c`)

- **Listar:** `opendir`/`readdir` + `lstat` por entrada; se muestran permisos (formato
  `-rwxr-xr-x`), tamano legible, fecha de modificacion y nombre coloreado por tipo.
- **Copiar:** copia recursiva; los archivos se copian con **I/O de bajo nivel**
  (`open`/`read`/`write`) en `util_copiar_archivo`.
- **Mover/renombrar:** intenta `rename()`; si falla por estar en distinto dispositivo,
  hace copia + borrado.
- **Borrar:** recursivo (`unlink` para archivos, `rmdir` para directorios) con confirmacion.
- **Buscar:** recorrido recursivo comparando por subcadena en el nombre.
- **Estadisticas:** conteo de archivos/directorios, tamano total y **top de extensiones**.

---

## 5. Modulo Comandos Linux (`comandos.c`)

Ejecuta comandos externos capturando **stdout y stderr por separado**:

1. Se crean dos `pipe()` (uno para stdout, otro para stderr).
2. `fork()`; en el hijo se redirige con `dup2()` cada pipe a `STDOUT_FILENO`/`STDERR_FILENO`
   y se ejecuta `execvp()`.
3. El padre lee **ambos** pipes con `poll()` (**E/S multiplexada**, Tema 15) para evitar
   bloqueos mutuos si uno de los flujos se llena.
4. `waitpid()` recupera el codigo de salida; se distingue salida normal (`WIFEXITED`)
   de terminacion por senal (`WIFSIGNALED`).

**Historial:** los comandos se guardan en memoria y se persisten en `~/.admin_history`,
permitiendo listarlos y re-ejecutarlos por indice.

---

## 6. Modulo Backups Incrementales (`backup.c`)

**Objetivo:** guardar solo lo que cambia entre snapshots (incremental) y poder restaurar
cualquier version.

**Estructura en disco:**
```
<almacen>/snapshots/<id>/data/<ruta_relativa>.gz   # bytes comprimidos
<almacen>/snapshots/<id>/manifest.txt              # inventario del snapshot
```
`id = snapshot_AAAAMMDD_HHMMSS`.

**Manifest** (una linea por archivo):
```
<ruta_relativa>\t<sha256>\t<holder_id>
```
`holder_id` es el snapshot que **fisicamente** contiene los bytes vigentes de ese archivo.

**Algoritmo incremental:**
1. Se recorre el origen y se calcula el **SHA-256** de cada archivo.
2. Se compara con el manifest del snapshot anterior:
   - Si el hash coincide -> se **reutiliza** el `holder_id` previo (no se copian bytes).
   - Si es nuevo o cambio -> se comprime a `data/` y el `holder_id` es el snapshot actual.
3. Se escribe el nuevo manifest con **todos** los archivos vigentes.

**Restauracion:** se lee el manifest del snapshot elegido y, por cada archivo, se
descomprime desde `snapshots/<holder_id>/data/...` hacia el destino. Tras escribir, se
**verifica la integridad** recomputando el SHA-256 y comparandolo con el del manifest.

**Compresion:** `zlib` via `gzopen`/`gzwrite`/`gzread`.

---

## 7. Modulo Analizador de Scripts Bash (`analizador.c`)

Analisis **estatico** linea por linea (sin ejecutar el script). Detecta:
- **Shebang** (`#!...`).
- **Variables:** asignaciones `NOMBRE=` (ignorando `==`), incluyendo `export`, `local`,
  `readonly`, `declare`; y **usos** con `$NOMBRE` / `${NOMBRE}`.
- **Ciclos:** `for`, `while`, `until`.
- **Condicionales:** `if`, `case`.
- **Funciones:** `function nombre` o `nombre() {`.
- **Comentarios** y total de lineas.

El parser evita falsos positivos exigiendo que las palabras clave aparezcan como
token inicial de la linea (seguidas de espacio/`;`/`(`).

---

## 8. Modulo Gestor de Descargas (`descargas.c`)

Combina **redes** (Tema 14) y **concurrencia con hilos** (Tema 15).

**Cliente HTTP sobre sockets TCP:**
- `getaddrinfo()` resuelve el host; `socket()` + `connect()` abren la conexion.
- Se envia una peticion `GET ... HTTP/1.1` con `Host` y `Connection: close`.
- Se parsea la linea de estado y las cabeceras; se soporta `Content-Length` y
  **codificacion `chunked`** (decodificada manualmente).
- El cuerpo se escribe al archivo destino en `descargas/`.

**Concurrencia:**
- Cada descarga corre en su propio hilo con `pthread_create` (worker).
- El estado, el progreso (`hecho`/`total`) y el registro de **eventos** son compartidos y
  se protegen con un **mutex** (`pthread_mutex_t`).
- El monitor en vivo refresca barras de progreso hasta que todas terminan.
- Se puede **cancelar** una descarga (bandera revisada por el worker bajo mutex).

**Integridad:** al finalizar, se calcula el **SHA-256** del archivo descargado.

> Limitacion consciente: solo `http://` (no TLS/`https://`), porque implementar TLS
> excede el alcance del curso. Se documenta y se rechaza `https://` con un mensaje claro.

---

## 9. SHA-256 propio (`util.c`)

Implementacion completa del algoritmo SHA-256 (FIPS 180-4) sin dependencias externas:
constantes `K`, funciones logicas (`Ch`, `Maj`, `Sigma`), expansion del mensaje y
compresion en 64 rondas. Se usa tanto para verificar backups como descargas.

---

## 10. Manejo de errores y robustez

- Entrada de usuario validada (`util_leer_entero`/`util_leer_texto`).
- Comprobacion de retornos de syscalls (`open`, `fork`, `pipe`, `connect`, etc.).
- `SIGPIPE` ignorado en `main` para que un socket cerrado no mate el proceso.
- Copias y escrituras manejan lecturas/escrituras parciales.

---

## 11. Decisiones de diseno

| Decision | Motivo |
|----------|--------|
| Leer `/proc` directamente | Evita depender de `ps`; muestra el uso real de syscalls |
| `poll()` para stdout/stderr | Evita bloqueos y demuestra E/S multiplexada |
| Incremental por hash (no solo mtime) | Mas robusto ante cambios de fecha sin contenido |
| Compresion gzip | Ahorra espacio y usa `zlib` (biblioteca compartida) |
| Un hilo por descarga | Modelo simple y correcto de concurrencia |
| SHA-256 propio | Refuerza "representacion de datos" y evita dependencias |

---

## 12. Posibles extensiones futuras

- Soporte `https://` con OpenSSL.
- Programacion de backups (cron interno con hilos).
- Limite de descargas concurrentes (pool de trabajadores).
- Exportar reportes (procesos/estadisticas) a CSV.
