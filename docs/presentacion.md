# Presentacion Final - ADMIN en Linux

> Guion de diapositivas + demo. Duracion sugerida: 8-10 minutos.

---

## Diapositiva 1 - Portada
- **ADMIN en Linux** - Herramienta de administracion de sistemas
- Curso: Programacion de Sistemas - UNSA 2026
- Integrantes del equipo / Fecha

## Diapositiva 2 - Objetivo
- Construir en **C** una herramienta que integre administracion de procesos, archivos,
  comandos, backups, analisis de bash y descargas de red.
- Aplicar directamente los temas del curso: syscalls, seniales, E/S, sockets, hilos.

## Diapositiva 3 - Arquitectura
- Diseno **modular**: `main` (menu) + 6 modulos + servicios comunes (`ui`, `util`).
- Compilacion separada con **Makefile** (`src/*.c -> obj/*.o -> admin`).
- Mostrar el diagrama de la documentacion tecnica.

## Diapositiva 4 - Modulo Procesos
- Lectura directa de `/proc` (sin usar `ps`).
- Calculo de %CPU por muestreo de jiffies; %memoria via `VmRSS`.
- Seniales: SIGTERM / SIGKILL / SIGSTOP / SIGCONT.
- Arbol de procesos por relacion PID/PPID.
- **Demo:** listar, suspender y reanudar un `sleep 300 &`.

## Diapositiva 5 - Shell de Archivos
- `opendir/readdir`, `stat`, copia con I/O de bajo nivel.
- Buscar y estadisticas (top de extensiones).
- **Demo:** estadisticas de `samples/`.

## Diapositiva 6 - Comandos
- `fork` + `execvp` + `pipe` + `dup2`.
- Captura **separada** de stdout y stderr con `poll()` (E/S multiplexada).
- Historial persistente.
- **Demo:** ejecutar `ls -la` y un comando invalido (ver stderr en rojo).

## Diapositiva 7 - Backups incrementales
- Snapshots + manifest con SHA-256 y `holder_id`.
- Solo se copian archivos cambiados (incremental) -> ahorro de espacio.
- Compresion gzip (`zlib`) y restauracion **verificada**.
- **Demo:** backup, modificar archivo, backup (reutiliza), restaurar.

## Diapositiva 8 - Analizador de scripts Bash
- Analisis estatico: variables, ciclos (for/while/until), condicionales, funciones.
- **Demo:** analizar `samples/ejemplo.sh`.

## Diapositiva 9 - Gestor de descargas (lo mas potente)
- Cliente HTTP propio sobre **sockets TCP**.
- **Concurrencia con hilos** (pthreads) + mutex + eventos.
- Barras de progreso en vivo e integridad SHA-256.
- **Demo:** `python3 -m http.server` y descargar varios archivos a la vez.

## Diapositiva 10 - Calidad y decisiones
- Manejo de errores, validacion de entrada, `SIGPIPE` ignorado.
- SHA-256 implementado desde cero (representacion de datos).
- Limitaciones conscientes (solo http, no TLS) y como se documentan.

## Diapositiva 11 - Mapeo con el silabo
- Procesos/seniales (Tema 10), E/S de sistema (Tema 13), redes (Tema 14),
  concurrencia (Tema 15), Makefile/compilacion separada (Tema 04), bash (Tema 02).

## Diapositiva 12 - Conclusiones
- Herramienta funcional, modular y extensible.
- Cumple todos los modulos del enunciado + extras (red, hilos, gzip, SHA-256).
- Trabajo en equipo versionado con Git.

## Diapositiva 13 - Preguntas
- Gracias. Espacio para preguntas.

---

### Checklist de la demo en vivo
1. `make` (mostrar compilacion limpia)
2. `./demo.sh` (prepara datos + servidor HTTP)
3. `./admin` -> recorrer modulos 1,3,4,5,6
4. Mostrar `~/.admin_history` y la carpeta `backups/`
