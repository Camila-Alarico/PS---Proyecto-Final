# Manual de Usuario - ADMIN en Linux

## 1. Que es

`ADMIN en Linux` es un programa de terminal para administrar tu sistema Linux desde un
unico menu: procesos, archivos, comandos, backups, analisis de scripts bash y descargas.

---

## 2. Instalacion y compilacion

```bash
# 1) Descomprimir el paquete
tar -xzf ADMIN-en-Linux.tar.gz     # o: unzip ADMIN-en-Linux.zip
cd admin-linux

# 2) (si falta zlib en Debian/Ubuntu)
sudo apt-get install build-essential zlib1g-dev

# 3) Compilar
make

# 4) Ejecutar
./admin
```

Para limpiar: `make clean`. Para recompilar desde cero: `make rebuild`.

---

## 3. Uso general

Al iniciar veras el **MENU PRINCIPAL**. Escribe el numero de la opcion y presiona ENTER.
En cada submenu, la opcion `0` regresa al menu anterior. `0` en el menu principal sale.

---

## 4. Modulo 1 - Administrador de Tareas

| Opcion | Descripcion |
|--------|-------------|
| 1 | Lista los procesos ordenados por uso de CPU (con memoria y estado) |
| 2 | Busca procesos por nombre (subcadena) |
| 3 | Termina un proceso (SIGTERM) |
| 4 | Fuerza la terminacion (SIGKILL) |
| 5 | Suspende un proceso (SIGSTOP) |
| 6 | Reanuda un proceso (SIGCONT) |
| 7 | Muestra el arbol de procesos desde un PID raiz |

> Para las opciones 3-6 se te pedira el **PID** del proceso.

**Ejemplo:** deja corriendo `sleep 300 &` en otra terminal, anota su PID y pruebalo con
las opciones 5 (suspender) y 6 (reanudar).

---

## 5. Modulo 2 - Shell de Archivos

| Opcion | Descripcion |
|--------|-------------|
| 1 | Listar el contenido de un directorio (estilo `ls -l`) |
| 2 | Cambiar de directorio de trabajo |
| 3 | Copiar un archivo o carpeta (recursivo) |
| 4 | Mover o renombrar |
| 5 | Borrar (recursivo, pide confirmacion con `s`) |
| 6 | Buscar por nombre dentro de un directorio (recursivo) |
| 7 | Estadisticas: numero de archivos, tamano total y top de extensiones |

---

## 6. Modulo 3 - Comandos Linux

| Opcion | Descripcion |
|--------|-------------|
| 1 | Ejecuta un comando; muestra la salida normal y los errores por separado |
| 2 | Muestra el historial de comandos ejecutados |
| 3 | Repite un comando del historial por su numero |

**Ejemplo:** opcion 1 y escribe `ls -la /etc`. Prueba tambien un comando invalido para
ver como se muestran los errores (stderr) en rojo y el codigo de salida.

El historial se guarda en `~/.admin_history`.

---

## 7. Modulo 4 - Backups Incrementales

| Opcion | Descripcion |
|--------|-------------|
| 1 | Crea un backup incremental de una carpeta |
| 2 | Lista los snapshots existentes |
| 3 | Restaura un snapshot en una carpeta destino |
| 4 | Cambia la carpeta "almacen" donde se guardan los backups (por defecto `backups/`) |

**Flujo recomendado:**
1. Opcion 1 -> carpeta origen `samples/datos`.
2. Modifica algun archivo de `samples/datos` y repite la opcion 1: veras que solo se
   respaldan los archivos cambiados (incremental).
3. Opcion 2 para ver los snapshots (copia el nombre completo, ej. `snapshot_20260101_120000`).
4. Opcion 3 -> pega el nombre del snapshot y una carpeta destino (ej. `/tmp/restaurado`).
   Al restaurar se **verifica la integridad** con SHA-256.

---

## 8. Modulo 5 - Analizador de Scripts Bash

| Opcion | Descripcion |
|--------|-------------|
| 1 | Analiza un archivo `.sh` y reporta variables, ciclos, condicionales y funciones |

**Ejemplo:** opcion 1 y escribe `samples/ejemplo.sh`.

---

## 9. Modulo 6 - Gestor de Descargas

| Opcion | Descripcion |
|--------|-------------|
| 1 | Agrega una URL `http://...` a la cola |
| 2 | Inicia todas las descargas en paralelo (hilos) |
| 3 | Monitor de progreso en vivo (barras que se actualizan) |
| 4 | Ver estado de la cola (incluye SHA-256 al completar) |
| 5 | Cancelar una descarga por su numero |

> Solo se admiten URLs `http://` (no `https://`).

**Ejemplo local (sin internet):** en otra terminal, dentro de una carpeta con archivos:
```bash
python3 -m http.server 8099
```
Luego en el programa: opcion 1 -> `http://127.0.0.1:8099/archivo.bin`, opcion 2 (iniciar),
opcion 3 (monitor).

---

## 10. Utilidad - Checksum SHA-256

Opcion 7 del menu principal: calcula el SHA-256 de cualquier archivo que indiques.

---

## 11. Problemas comunes

| Problema | Solucion |
|----------|----------|
| `zlib.h: No such file` al compilar | Instala `zlib1g-dev` |
| "HTTPS no soportado" | Usa una URL `http://` |
| "No se pudo enviar la senal" | Verifica el PID y que tengas permisos |
| No aparecen colores | Usa una terminal compatible con ANSI (la mayoria lo son) |
