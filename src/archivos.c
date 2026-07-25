#include "archivos.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ------- Listar directorio (estilo ls -l) ------- */
static void listar(const char *ruta) {
    DIR *d = opendir(ruta);
    if (!d) { ui_error("No se pudo abrir '%s': %s", ruta, strerror(errno)); return; }
    struct dirent *e;
    int total = 0;
    printf(C_BOLD "%-11s %10s  %-16s %s\n" C_RESET, "PERMISOS", "TAMANO", "MODIFICADO", "NOMBRE");
    ui_linea();
    while ((e = readdir(d))) {
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", ruta, e->d_name);
        struct stat st;
        if (lstat(full, &st) != 0) continue;
        char modo[11]; util_modo_texto(st.st_mode, modo);
        char tam[32];  util_tam_legible((unsigned long)st.st_size, tam, sizeof(tam));
        char fecha[20];
        struct tm *tm = localtime(&st.st_mtime);
        strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M", tm);
        const char *color = S_ISDIR(st.st_mode) ? C_BLUE C_BOLD :
                            (st.st_mode & S_IXUSR ? C_GREEN : C_RESET);
        printf("%s %10s  %-16s %s%s\n" C_RESET, modo, tam, fecha, color, e->d_name);
        total++;
    }
    closedir(d);
    ui_linea();
    ui_info("%d entradas en '%s'.", total, ruta);
}

/* ------- Borrado recursivo ------- */
static int borrar_rec(const char *ruta) {
    struct stat st;
    if (lstat(ruta, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(ruta);
        if (!d) return -1;
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", ruta, e->d_name);
            borrar_rec(full);
        }
        closedir(d);
        return rmdir(ruta);
    }
    return unlink(ruta);
}

/* ------- Copia recursiva ------- */
static int copiar_rec(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST) return -1;
        DIR *d = opendir(src);
        if (!d) return -1;
        struct dirent *e;
        int rc = 0;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char s2[2048], d2[2048];
            snprintf(s2, sizeof(s2), "%s/%s", src, e->d_name);
            snprintf(d2, sizeof(d2), "%s/%s", dst, e->d_name);
            if (copiar_rec(s2, d2) != 0) rc = -1;
        }
        closedir(d);
        return rc;
    }
    return util_copiar_archivo(src, dst);
}

/* ------- Busqueda recursiva por subcadena ------- */
static int buscar_rec(const char *ruta, const char *patron, int *contador) {
    DIR *d = opendir(ruta);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", ruta, e->d_name);
        if (strstr(e->d_name, patron)) {
            printf(C_GREEN "  %s\n" C_RESET, full);
            (*contador)++;
        }
        struct stat st;
        if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode))
            buscar_rec(full, patron, contador);
    }
    closedir(d);
    return 0;
}

/* ------- Estadisticas recursivas ------- */
typedef struct { char ext[16]; long cuenta; unsigned long bytes; } ExtStat;

static void stats_rec(const char *ruta, long *archivos, long *dirs,
                      unsigned long *bytes, ExtStat *exts, int *nexts) {
    DIR *d = opendir(ruta);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", ruta, e->d_name);
        struct stat st;
        if (lstat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            (*dirs)++;
            stats_rec(full, archivos, dirs, bytes, exts, nexts);
        } else {
            (*archivos)++;
            *bytes += (unsigned long)st.st_size;
            const char *punto = strrchr(e->d_name, '.');
            char ext[16] = "(sin ext)";
            if (punto && punto != e->d_name) {
                strncpy(ext, punto, sizeof(ext)-1);
                ext[sizeof(ext)-1] = '\0';
            }
            int found = 0;
            for (int i = 0; i < *nexts; ++i) {
                if (strcmp(exts[i].ext, ext) == 0) {
                    exts[i].cuenta++; exts[i].bytes += (unsigned long)st.st_size;
                    found = 1; break;
                }
            }
            if (!found && *nexts < 256) {
                strncpy(exts[*nexts].ext, ext, sizeof(exts[*nexts].ext)-1);
                exts[*nexts].ext[sizeof(exts[*nexts].ext)-1] = '\0';
                exts[*nexts].cuenta = 1;
                exts[*nexts].bytes = (unsigned long)st.st_size;
                (*nexts)++;
            }
        }
    }
    closedir(d);
}

static int cmp_ext(const void *a, const void *b) {
    return (int)(((const ExtStat*)b)->cuenta - ((const ExtStat*)a)->cuenta);
}

static void accion_stats(const char *ruta) {
    static ExtStat exts[256];
    int nexts = 0;
    long archivos = 0, dirs = 0;
    unsigned long bytes = 0;
    stats_rec(ruta, &archivos, &dirs, &bytes, exts, &nexts);
    char tam[32]; util_tam_legible(bytes, tam, sizeof(tam));
    ui_linea();
    ui_info("Directorio analizado: %s", ruta);
    printf("  Archivos totales : " C_BOLD "%ld\n" C_RESET, archivos);
    printf("  Directorios      : " C_BOLD "%ld\n" C_RESET, dirs);
    printf("  Tamano total     : " C_BOLD "%s\n\n" C_RESET, tam);
    qsort(exts, nexts, sizeof(ExtStat), cmp_ext);
    printf(C_BOLD "  Top extensiones:\n" C_RESET);
    int top = nexts < 8 ? nexts : 8;
    for (int i = 0; i < top; ++i) {
        char t[32]; util_tam_legible(exts[i].bytes, t, sizeof(t));
        printf("   %-12s %5ld archivos  (%s)\n", exts[i].ext, exts[i].cuenta, t);
    }
}

void archivos_menu(void) {
    int salir = 0;
    char cwd[1024];
    while (!salir) {
        if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
        ui_encabezado("Shell de Archivos");
        printf(C_DIM "  Directorio actual: %s\n\n" C_RESET, cwd);
        printf("  " C_BOLD "1)" C_RESET " Listar (ls) de un directorio\n");
        printf("  " C_BOLD "2)" C_RESET " Cambiar de directorio (cd)\n");
        printf("  " C_BOLD "3)" C_RESET " Copiar archivo/carpeta\n");
        printf("  " C_BOLD "4)" C_RESET " Mover / renombrar\n");
        printf("  " C_BOLD "5)" C_RESET " Borrar (recursivo)\n");
        printf("  " C_BOLD "6)" C_RESET " Buscar por nombre (recursivo)\n");
        printf("  " C_BOLD "7)" C_RESET " Estadisticas del directorio\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        char a[1024], b[1024];
        switch (op) {
            case 1:
                util_leer_texto("Ruta (ENTER=actual): ", a, sizeof(a));
                listar(a[0] ? a : "."); ui_pausa(); break;
            case 2:
                util_leer_texto("Nueva ruta: ", a, sizeof(a));
                if (chdir(a) == 0) ui_ok("Directorio cambiado.");
                else ui_error("No se pudo cambiar: %s", strerror(errno));
                ui_pausa(); break;
            case 3:
                util_leer_texto("Origen: ", a, sizeof(a));
                util_leer_texto("Destino: ", b, sizeof(b));
                if (copiar_rec(a, b) == 0) ui_ok("Copiado correctamente.");
                else ui_error("Fallo la copia: %s", strerror(errno));
                ui_pausa(); break;
            case 4:
                util_leer_texto("Origen: ", a, sizeof(a));
                util_leer_texto("Destino: ", b, sizeof(b));
                if (rename(a, b) == 0) ui_ok("Movido/renombrado.");
                else if (copiar_rec(a, b) == 0 && borrar_rec(a) == 0) ui_ok("Movido (copia+borrado entre dispositivos).");
                else ui_error("Fallo al mover: %s", strerror(errno));
                ui_pausa(); break;
            case 5:
                util_leer_texto("Ruta a borrar: ", a, sizeof(a));
                util_leer_texto("Confirmar borrado de arriba? (s/N): ", b, sizeof(b));
                if (b[0] == 's' || b[0] == 'S') {
                    if (borrar_rec(a) == 0) ui_ok("Borrado.");
                    else ui_error("Fallo el borrado: %s", strerror(errno));
                } else ui_advertencia("Cancelado.");
                ui_pausa(); break;
            case 6: {
                util_leer_texto("Directorio base: ", a, sizeof(a));
                util_leer_texto("Patron (subcadena): ", b, sizeof(b));
                ui_info("Resultados:");
                int c = 0;
                buscar_rec(a[0] ? a : ".", b, &c);
                ui_info("%d coincidencia(s).", c);
                ui_pausa(); break;
            }
            case 7:
                util_leer_texto("Directorio (ENTER=actual): ", a, sizeof(a));
                accion_stats(a[0] ? a : "."); ui_pausa(); break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}
