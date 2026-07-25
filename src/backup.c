#include "backup.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <zlib.h>

/*
 * Sistema de backups incrementales.
 *
 * Estructura en <backup_root>:
 *   snapshots/<id>/data/<rutas...>.gz   -> bytes comprimidos (solo archivos nuevos/cambiados)
 *   snapshots/<id>/manifest.txt          -> lista de TODOS los archivos vigentes en ese snapshot
 *
 * Formato manifest (una linea por archivo):
 *   <relpath>\t<sha256>\t<holder_id>
 *   holder_id = id del snapshot que fisicamente guarda los bytes actuales.
 *
 * Incremental: si el hash no cambio respecto al snapshot anterior, se reutiliza
 * el 'holder_id' previo (no se copian bytes). Esto ahorra espacio.
 */

typedef struct {
    char rel[1024];
    char hash[65];
    char holder[32];
} Entrada;

/* Comprime src -> dst usando gzip (zlib) */
static int gz_comprimir(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    gzFile out = gzopen(dst, "wb6");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), in)) > 0)
        if (gzwrite(out, buf, (unsigned)r) == 0) { fclose(in); gzclose(out); return -1; }
    fclose(in);
    gzclose(out);
    return 0;
}

/* Descomprime gzip src -> dst */
static int gz_descomprimir(const char *src, const char *dst) {
    gzFile in = gzopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { gzclose(in); return -1; }
    char buf[65536];
    int r;
    while ((r = gzread(in, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, (size_t)r, out);
    gzclose(in);
    fclose(out);
    return 0;
}

/* Encuentra el snapshot mas reciente (ids con formato ordenable por nombre) */
static int ultimo_snapshot(const char *root, char *out, size_t n) {
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/snapshots", root);
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *e;
    char mejor[256] = "";
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (strcmp(e->d_name, mejor) > 0) {
            strncpy(mejor, e->d_name, sizeof(mejor)-1);
            mejor[sizeof(mejor)-1] = '\0';
        }
    }
    closedir(d);
    if (!mejor[0]) return -1;
    strncpy(out, mejor, n-1); out[n-1]='\0';
    return 0;
}

static int cargar_manifest(const char *root, const char *id, Entrada *arr, int max) {
    char ruta[1400];
    snprintf(ruta, sizeof(ruta), "%s/snapshots/%s/manifest.txt", root, id);
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;
    int n = 0;
    char linea[1300];
    while (n < max && fgets(linea, sizeof(linea), f)) {
        size_t l = strlen(linea);
        if (l && linea[l-1]=='\n') linea[l-1]='\0';
        char *t1 = strchr(linea, '\t');
        if (!t1) continue;
        *t1 = '\0';
        char *t2 = strchr(t1+1, '\t');
        if (!t2) continue;
        *t2 = '\0';
        strncpy(arr[n].rel, linea, sizeof(arr[n].rel)-1); arr[n].rel[sizeof(arr[n].rel)-1]='\0';
        strncpy(arr[n].hash, t1+1, sizeof(arr[n].hash)-1); arr[n].hash[sizeof(arr[n].hash)-1]='\0';
        strncpy(arr[n].holder, t2+1, sizeof(arr[n].holder)-1); arr[n].holder[sizeof(arr[n].holder)-1]='\0';
        n++;
    }
    fclose(f);
    return n;
}

/* Recorre el arbol de origen y llena rutas relativas */
static void listar_archivos(const char *base, const char *rel,
                            char rutas[][1024], int *n, int max) {
    char full[2048];
    if (rel[0]) snprintf(full, sizeof(full), "%s/%s", base, rel);
    else snprintf(full, sizeof(full), "%s", base);
    DIR *d = opendir(full);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && *n < max) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char nrel[1024];
        if (rel[0]) snprintf(nrel, sizeof(nrel), "%s/%s", rel, e->d_name);
        else snprintf(nrel, sizeof(nrel), "%s", e->d_name);
        char nfull[2048];
        snprintf(nfull, sizeof(nfull), "%s/%s", base, nrel);
        struct stat st;
        if (lstat(nfull, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            listar_archivos(base, nrel, rutas, n, max);
        } else if (S_ISREG(st.st_mode)) {
            strncpy(rutas[*n], nrel, 1023); rutas[*n][1023]='\0';
            (*n)++;
        }
    }
    closedir(d);
}

#define MAX_FILES 20000

static void crear_backup(const char *src, const char *root) {
    if (!util_es_directorio(src)) { ui_error("El origen no es un directorio valido."); return; }

    /* id = snapshot_YYYYmmdd_HHMMSS */
    char id[64];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(id, sizeof(id), "snapshot_%Y%m%d_%H%M%S", tm);

    char prev[64] = "";
    int hay_prev = (ultimo_snapshot(root, prev, sizeof(prev)) == 0);

    static Entrada previas[MAX_FILES];
    int nprev = hay_prev ? cargar_manifest(root, prev, previas, MAX_FILES) : 0;

    static char rutas[MAX_FILES][1024];
    int nrut = 0;
    listar_archivos(src, "", rutas, &nrut, MAX_FILES);

    char dirdata[1500];
    snprintf(dirdata, sizeof(dirdata), "%s/snapshots/%s/data", root, id);
    util_crear_dirs(dirdata);

    char ruta_manifest[1500];
    snprintf(ruta_manifest, sizeof(ruta_manifest), "%s/snapshots/%s/manifest.txt", root, id);
    FILE *mf = fopen(ruta_manifest, "w");
    if (!mf) { ui_error("No se pudo crear el manifest: %s", strerror(errno)); return; }

    int nuevos = 0, reutilizados = 0;
    unsigned long bytes_copiados = 0;
    for (int i = 0; i < nrut; ++i) {
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", src, rutas[i]);
        char h[65];
        if (util_sha256_archivo(full, h) != 0) continue;

        /* Buscar en manifest previo */
        const char *holder = NULL;
        for (int j = 0; j < nprev; ++j) {
            if (strcmp(previas[j].rel, rutas[i]) == 0 &&
                strcmp(previas[j].hash, h) == 0) {
                holder = previas[j].holder;
                break;
            }
        }
        if (holder) {
            reutilizados++;
            fprintf(mf, "%s\t%s\t%s\n", rutas[i], h, holder);
        } else {
            /* Copiar (comprimido) a este snapshot */
            char destino[3600];
            snprintf(destino, sizeof(destino), "%s/%s.gz", dirdata, rutas[i]);
            /* crear subdirectorios del destino */
            char dcopy[3600]; strncpy(dcopy, destino, sizeof(dcopy)-1); dcopy[sizeof(dcopy)-1]='\0';
            char *ultima = strrchr(dcopy, '/');
            if (ultima) { *ultima = '\0'; util_crear_dirs(dcopy); }
            if (gz_comprimir(full, destino) == 0) {
                nuevos++;
                struct stat st; if (stat(full, &st) == 0) bytes_copiados += (unsigned long)st.st_size;
                fprintf(mf, "%s\t%s\t%s\n", rutas[i], h, id);
            }
        }
    }
    fclose(mf);
    char tam[32]; util_tam_legible(bytes_copiados, tam, sizeof(tam));
    ui_ok("Snapshot creado: %s", id);
    ui_info("Archivos totales: %d | Nuevos/cambiados: %d | Reutilizados (incremental): %d",
            nrut, nuevos, reutilizados);
    ui_info("Datos nuevos comprimidos (origen): %s", tam);
}

static void listar_snapshots(const char *root) {
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/snapshots", root);
    DIR *d = opendir(dir);
    if (!d) { ui_advertencia("Aun no hay snapshots en '%s'.", root); return; }
    struct dirent *e;
    char nombres[512][256];
    int n = 0;
    while ((e = readdir(d)) && n < 512) {
        if (e->d_name[0] == '.') continue;
        strncpy(nombres[n], e->d_name, 255); nombres[n][255]='\0'; n++;
    }
    closedir(d);
    /* ordenar alfabeticamente (cronologico por el formato de nombre) */
    for (int i = 0; i < n; ++i)
        for (int j = i+1; j < n; ++j)
            if (strcmp(nombres[i], nombres[j]) > 0) {
                char tmp[256]; strcpy(tmp, nombres[i]); strcpy(nombres[i], nombres[j]); strcpy(nombres[j], tmp);
            }
    ui_info("Snapshots disponibles (%d):", n);
    for (int i = 0; i < n; ++i) {
        static Entrada arr[MAX_FILES];
        int c = cargar_manifest(root, nombres[i], arr, MAX_FILES);
        printf("  " C_BOLD "%2d)" C_RESET " %s  " C_DIM "(%d archivos)\n" C_RESET, i+1, nombres[i], c);
    }
}

static void restaurar(const char *root) {
    listar_snapshots(root);
    char id[256];
    util_leer_texto("\nID del snapshot a restaurar (nombre completo): ", id, sizeof(id));
    if (!id[0]) { ui_advertencia("Cancelado."); return; }
    char dest[1024];
    util_leer_texto("Carpeta destino para restaurar: ", dest, sizeof(dest));
    if (!dest[0]) { ui_advertencia("Cancelado."); return; }

    static Entrada arr[MAX_FILES];
    int n = cargar_manifest(root, id, arr, MAX_FILES);
    if (n == 0) { ui_error("Snapshot vacio o inexistente."); return; }
    util_crear_dirs(dest);
    int ok = 0;
    for (int i = 0; i < n; ++i) {
        char origen[3600], destino[2400];
        snprintf(origen, sizeof(origen), "%s/snapshots/%s/data/%s.gz", root, arr[i].holder, arr[i].rel);
        snprintf(destino, sizeof(destino), "%s/%s", dest, arr[i].rel);
        char dcopy[2400]; strncpy(dcopy, destino, sizeof(dcopy)-1); dcopy[sizeof(dcopy)-1]='\0';
        char *ult = strrchr(dcopy, '/');
        if (ult) { *ult='\0'; util_crear_dirs(dcopy); }
        if (gz_descomprimir(origen, destino) == 0) {
            /* verificar integridad */
            char h[65];
            if (util_sha256_archivo(destino, h) == 0 && strcmp(h, arr[i].hash) == 0) ok++;
        }
    }
    ui_ok("Restaurados %d de %d archivos en '%s' (integridad SHA-256 verificada).", ok, n, dest);
}

void backup_menu(void) {
    static char root[1024] = "";
    if (!root[0]) snprintf(root, sizeof(root), "backups");
    int salir = 0;
    while (!salir) {
        ui_encabezado("Backups Incrementales");
        printf(C_DIM "  Almacen de backups: %s\n\n" C_RESET, root);
        printf("  " C_BOLD "1)" C_RESET " Crear backup incremental de una carpeta\n");
        printf("  " C_BOLD "2)" C_RESET " Listar snapshots\n");
        printf("  " C_BOLD "3)" C_RESET " Restaurar un snapshot\n");
        printf("  " C_BOLD "4)" C_RESET " Cambiar carpeta de almacen\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        char a[1024];
        switch (op) {
            case 1:
                util_leer_texto("Carpeta origen a respaldar: ", a, sizeof(a));
                if (a[0]) crear_backup(a, root);
                ui_pausa(); break;
            case 2: listar_snapshots(root); ui_pausa(); break;
            case 3: restaurar(root); ui_pausa(); break;
            case 4:
                util_leer_texto("Nueva carpeta de almacen: ", a, sizeof(a));
                if (a[0]) { strncpy(root, a, sizeof(root)-1); root[sizeof(root)-1]='\0'; ui_ok("Almacen actualizado."); }
                ui_pausa(); break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}
