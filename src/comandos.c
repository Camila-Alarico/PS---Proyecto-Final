#include "comandos.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>

#define MAX_ARGS 64
#define HIST_MAX 100
#define BUF_MAX 65536   /* tamano maximo guardado de la ultima salida/error */

static char historial[HIST_MAX][256];
static int  hist_n = 0;

/* Guardan la salida y el error del ULTIMO comando ejecutado */
static char ultima_salida[BUF_MAX] = "";
static char ultimo_error[BUF_MAX]  = "";
static char ultimo_cmd[256] = "";

static void hist_ruta(char *out, size_t n) {
    const char *home = getenv("HOME");
    snprintf(out, n, "%s/.admin_history", home ? home : ".");
}

static void hist_cargar(void) {
    char ruta[512]; hist_ruta(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "r");
    if (!f) return;
    char linea[256];
    while (hist_n < HIST_MAX && fgets(linea, sizeof(linea), f)) {
        size_t l = strlen(linea);
        if (l && linea[l-1] == '\n') linea[l-1] = '\0';
        if (linea[0]) { strncpy(historial[hist_n], linea, 255); historial[hist_n][255]='\0'; hist_n++; }
    }
    fclose(f);
}

static void hist_agregar(const char *cmd) {
    if (hist_n >= HIST_MAX) {
        memmove(historial[0], historial[1], (HIST_MAX-1)*256);
        hist_n = HIST_MAX - 1;
    }
    strncpy(historial[hist_n], cmd, 255);
    historial[hist_n][255] = '\0';
    hist_n++;
    char ruta[512]; hist_ruta(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "a");
    if (f) { fprintf(f, "%s\n", cmd); fclose(f); }
}

/* Divide una linea en argv (tokenizacion simple por espacios) */
static int tokenizar(char *linea, char **argv) {
    int n = 0;
    char *tok = strtok(linea, " \t");
    while (tok && n < MAX_ARGS - 1) {
        argv[n++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[n] = NULL;
    return n;
}

/* Agrega texto a un buffer sin desbordarlo */
static void buf_append(char *dest, size_t dest_size, const char *texto) {
    size_t usado = strlen(dest);
    if (usado >= dest_size - 1) return;
    strncat(dest, texto, dest_size - usado - 1);
}

/*
 * Ejecuta un comando con fork/execvp capturando stdout y stderr por separado.
 * Usa poll() (E/S multiplexada) para leer ambos pipes sin bloqueo mutuo.
 * Ademas guarda la salida y el error completos en buffers para poder
 * volver a mostrarlos despues desde el menu.
 */
static void ejecutar(const char *cmdline) {
    char copia[256];
    strncpy(copia, cmdline, sizeof(copia)-1);
    copia[sizeof(copia)-1] = '\0';
    char *argv[MAX_ARGS];
    if (tokenizar(copia, argv) == 0) { ui_advertencia("Comando vacio."); return; }

    /* Reiniciar buffers del ultimo comando */
    ultima_salida[0] = '\0';
    ultimo_error[0]  = '\0';
    strncpy(ultimo_cmd, cmdline, sizeof(ultimo_cmd)-1);
    ultimo_cmd[sizeof(ultimo_cmd)-1] = '\0';

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        ui_error("No se pudieron crear pipes: %s", strerror(errno));
        return;
    }
    pid_t pid = fork();
    if (pid < 0) { ui_error("fork fallo: %s", strerror(errno)); return; }
    if (pid == 0) {
        /* Hijo: redirigir stdout y stderr a los pipes */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        execvp(argv[0], argv);
        /* Si execvp falla */
        fprintf(stderr, "execvp: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    /* Padre */
    close(out_pipe[1]);
    close(err_pipe[1]);

    struct pollfd fds[2];
    fds[0].fd = out_pipe[0]; fds[0].events = POLLIN;
    fds[1].fd = err_pipe[0]; fds[1].events = POLLIN;
    int abiertos = 2;
    char buf[4096];
    printf(C_BOLD "--- salida estandar (stdout) ---\n" C_RESET);
    int hubo_err = 0;
    while (abiertos > 0) {
        int r = poll(fds, 2, -1);
        if (r < 0) { if (errno == EINTR) continue; break; }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0) continue;
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                ssize_t n = read(fds[i].fd, buf, sizeof(buf)-1);
                if (n > 0) {
                    buf[n] = '\0';
                    if (i == 0) {
                        fputs(buf, stdout);
                        buf_append(ultima_salida, sizeof(ultima_salida), buf);
                    } else {
                        if (!hubo_err) { printf(C_RED "\n--- errores (stderr) ---\n"); hubo_err = 1; }
                        printf(C_RED "%s" C_RESET, buf);
                        buf_append(ultimo_error, sizeof(ultimo_error), buf);
                    }
                } else {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    abiertos--;
                }
            }
        }
    }
    int estado = 0;
    waitpid(pid, &estado, 0);
    printf(C_RESET);
    ui_linea();
    if (WIFEXITED(estado))
        ui_info("Comando finalizado con codigo de salida: %d", WEXITSTATUS(estado));
    else if (WIFSIGNALED(estado))
        ui_advertencia("Comando terminado por senal: %d", WTERMSIG(estado));
    hist_agregar(cmdline);
}

/* Muestra de nuevo la salida (stdout) del ultimo comando ejecutado */
static void mostrar_ultima_salida(void) {
    ui_encabezado("Ultima Salida (stdout)");
    if (ultimo_cmd[0] == '\0') { ui_info("Aun no se ha ejecutado ningun comando."); return; }
    ui_info("Comando: %s", ultimo_cmd);
    ui_linea();
    if (ultima_salida[0] == '\0') ui_info("(sin salida estandar)");
    else fputs(ultima_salida, stdout);
}

/* Muestra de nuevo los errores (stderr) del ultimo comando ejecutado */
static void mostrar_ultimo_error(void) {
    ui_encabezado("Ultimos Errores (stderr)");
    if (ultimo_cmd[0] == '\0') { ui_info("Aun no se ha ejecutado ningun comando."); return; }
    ui_info("Comando: %s", ultimo_cmd);
    ui_linea();
    if (ultimo_error[0] == '\0') ui_info("(sin errores)");
    else printf(C_RED "%s" C_RESET, ultimo_error);
}

static void mostrar_historial(void) {
    ui_encabezado("Historial de Comandos");
    if (hist_n == 0) { ui_info("Historial vacio."); return; }
    for (int i = 0; i < hist_n; ++i)
        printf("  %3d  %s\n", i + 1, historial[i]);
}

void comandos_menu(void) {
    static int cargado = 0;
    if (!cargado) { hist_cargar(); cargado = 1; }
    int salir = 0;
    while (!salir) {
        ui_encabezado("Ejecucion de Comandos Linux");
        printf("  " C_BOLD "1)" C_RESET " Ejecutar un comando\n");
        printf("  " C_BOLD "2)" C_RESET " Mostrar salida (ultimo comando)\n");
        printf("  " C_BOLD "3)" C_RESET " Mostrar errores (ultimo comando)\n");
        printf("  " C_BOLD "4)" C_RESET " Ver historial\n");
        printf("  " C_BOLD "5)" C_RESET " Repetir comando del historial\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        char cmd[256];
        switch (op) {
            case 1:
                util_leer_texto("$ ", cmd, sizeof(cmd));
                if (cmd[0]) { ui_linea(); ejecutar(cmd); }
                ui_pausa(); break;
            case 2: mostrar_ultima_salida(); ui_pausa(); break;
            case 3: mostrar_ultimo_error(); ui_pausa(); break;
            case 4: mostrar_historial(); ui_pausa(); break;
            case 5: {
                mostrar_historial();
                long idx;
                if (util_leer_entero("\nNumero a repetir: ", &idx) == 0 &&
                    idx >= 1 && idx <= hist_n) {
                    char c[256]; strncpy(c, historial[idx-1], sizeof(c)-1); c[sizeof(c)-1]='\0';
                    ui_linea(); ejecutar(c);
                } else ui_advertencia("Indice invalido.");
                ui_pausa(); break;
            }
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}