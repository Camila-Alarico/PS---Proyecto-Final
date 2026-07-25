#include "descargas.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

/*
 * Gestor de descargas concurrente.
 * - Cliente HTTP/1.1 sobre sockets TCP (Tema 14: programacion de redes).
 * - Cada descarga corre en su propio hilo (Tema 15: concurrencia con threads).
 * - Estado y progreso protegidos con mutex; registro de eventos compartido.
 */

#define MAX_DESC 32
#define MAX_EVENTOS 200

enum { PENDIENTE, DESCARGANDO, COMPLETADA, ERROR, CANCELADA };

typedef struct {
    int   usado;
    char  url[1024];
    char  host[256];
    char  path[1024];
    int   port;
    char  salida[512];
    long  total;      /* -1 si desconocido */
    long  hecho;
    int   estado;
    int   cancelar;
    char  sha[65];
    pthread_t tid;
} Descarga;

static Descarga g_desc[MAX_DESC];
static int      g_ndesc = 0;
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

static char g_eventos[MAX_EVENTOS][160];
static int  g_nev = 0;

static const char *estado_txt(int e) {
    switch (e) {
        case PENDIENTE:   return "PENDIENTE";
        case DESCARGANDO: return "DESCARGANDO";
        case COMPLETADA:  return "COMPLETADA";
        case ERROR:       return "ERROR";
        case CANCELADA:   return "CANCELADA";
    }
    return "?";
}

static void evento(const char *fmt, ...) {
    char tmp[160];
    va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char hora[16]; strftime(hora, sizeof(hora), "%H:%M:%S", tm);
    pthread_mutex_lock(&g_mtx);
    if (g_nev < MAX_EVENTOS)
        snprintf(g_eventos[g_nev++], 160, "[%s] %s", hora, tmp);
    pthread_mutex_unlock(&g_mtx);
}

/* Parsea http://host[:puerto]/ruta */
static int parse_url(const char *url, char *host, int *port, char *path) {
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) return -2; /* HTTPS no soportado (sin TLS) */
    *port = 80;
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hl = (size_t)(colon - p);
        strncpy(host, p, hl); host[hl] = '\0';
        *port = atoi(colon + 1);
    } else {
        size_t hl = slash ? (size_t)(slash - p) : strlen(p);
        strncpy(host, p, hl); host[hl] = '\0';
    }
    if (slash) { strncpy(path, slash, 1023); path[1023]='\0'; }
    else strcpy(path, "/");
    return 0;
}

static int conectar(const char *host, int port) {
    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;
    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Lee una linea terminada en \r\n del socket (byte a byte) */
static int leer_linea_sock(int fd, char *buf, size_t n) {
    size_t i = 0;
    while (i < n - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return (i > 0) ? (int)i : -1;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

static void *worker(void *arg) {
    Descarga *d = (Descarga *)arg;
    pthread_mutex_lock(&g_mtx); d->estado = DESCARGANDO; pthread_mutex_unlock(&g_mtx);
    evento("Iniciando: %s", d->url);

    int fd = conectar(d->host, d->port);
    if (fd < 0) {
        pthread_mutex_lock(&g_mtx); d->estado = ERROR; pthread_mutex_unlock(&g_mtx);
        evento("ERROR de conexion: %s:%d", d->host, d->port);
        return NULL;
    }

    char req[2048];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: ADMIN-Linux/1.0\r\n"
        "Accept: */*\r\nConnection: close\r\n\r\n", d->path, d->host);
    if (write(fd, req, strlen(req)) < 0) {
        close(fd);
        pthread_mutex_lock(&g_mtx); d->estado = ERROR; pthread_mutex_unlock(&g_mtx);
        evento("ERROR al enviar peticion");
        return NULL;
    }

    /* Leer status line */
    char linea[4096];
    if (leer_linea_sock(fd, linea, sizeof(linea)) <= 0) {
        close(fd);
        pthread_mutex_lock(&g_mtx); d->estado = ERROR; pthread_mutex_unlock(&g_mtx);
        evento("ERROR: sin respuesta del servidor");
        return NULL;
    }
    int codigo = 0;
    sscanf(linea, "HTTP/%*s %d", &codigo);

    long content_length = -1;
    int chunked = 0;
    /* Leer cabeceras */
    while (leer_linea_sock(fd, linea, sizeof(linea)) > 0) {
        if (strncasecmp(linea, "Content-Length:", 15) == 0)
            content_length = atol(linea + 15);
        else if (strncasecmp(linea, "Transfer-Encoding:", 18) == 0 &&
                 strstr(linea, "chunked"))
            chunked = 1;
    }

    if (codigo < 200 || codigo >= 300) {
        close(fd);
        pthread_mutex_lock(&g_mtx); d->estado = ERROR; pthread_mutex_unlock(&g_mtx);
        evento("HTTP %d en %s", codigo, d->url);
        return NULL;
    }

    pthread_mutex_lock(&g_mtx); d->total = content_length; pthread_mutex_unlock(&g_mtx);

    FILE *out = fopen(d->salida, "wb");
    if (!out) {
        close(fd);
        pthread_mutex_lock(&g_mtx); d->estado = ERROR; pthread_mutex_unlock(&g_mtx);
        evento("ERROR: no se pudo crear %s", d->salida);
        return NULL;
    }

    char buf[65536];
    long recibido = 0;
    int cancelado = 0;

    if (chunked) {
        /* Decodificacion chunked minima */
        while (1) {
            if (leer_linea_sock(fd, linea, sizeof(linea)) < 0) break;
            long tam = strtol(linea, NULL, 16);
            if (tam <= 0) break;
            long resta = tam;
            while (resta > 0) {
                ssize_t r = read(fd, buf, resta < (long)sizeof(buf) ? (size_t)resta : sizeof(buf));
                if (r <= 0) break;
                fwrite(buf, 1, (size_t)r, out);
                resta -= r; recibido += r;
                pthread_mutex_lock(&g_mtx); d->hecho = recibido;
                cancelado = d->cancelar; pthread_mutex_unlock(&g_mtx);
                if (cancelado) break;
            }
            leer_linea_sock(fd, linea, sizeof(linea)); /* CRLF final del chunk */
            if (cancelado) break;
        }
    } else {
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, (size_t)r, out);
            recibido += r;
            pthread_mutex_lock(&g_mtx); d->hecho = recibido;
            cancelado = d->cancelar; pthread_mutex_unlock(&g_mtx);
            if (cancelado) break;
            if (content_length > 0 && recibido >= content_length) break;
        }
    }
    fclose(out);
    close(fd);

    if (cancelado) {
        pthread_mutex_lock(&g_mtx); d->estado = CANCELADA; pthread_mutex_unlock(&g_mtx);
        evento("CANCELADA: %s", d->url);
        return NULL;
    }

    /* Integridad: SHA-256 del archivo descargado */
    util_sha256_archivo(d->salida, d->sha);
    pthread_mutex_lock(&g_mtx);
    d->estado = COMPLETADA;
    if (d->total < 0) d->total = recibido;
    pthread_mutex_unlock(&g_mtx);
    evento("COMPLETADA: %s (%ld bytes)", d->salida, recibido);
    return NULL;
}

static void agregar(void) {
    if (g_ndesc >= MAX_DESC) { ui_error("Cola llena."); return; }
    char url[1024];
    util_leer_texto("URL (http://...): ", url, sizeof(url));
    if (!url[0]) return;
    Descarga *d = &g_desc[g_ndesc];
    memset(d, 0, sizeof(*d));
    if (parse_url(url, d->host, &d->port, d->path) != 0) {
        ui_error("URL invalida o HTTPS no soportado (usa http://).");
        return;
    }
    strncpy(d->url, url, sizeof(d->url)-1);
    /* nombre de archivo por defecto */
    const char *base = strrchr(d->path, '/');
    base = (base && base[1]) ? base + 1 : "descarga.bin";
    util_crear_dirs("descargas");
    snprintf(d->salida, sizeof(d->salida), "descargas/%s", base);
    d->total = -1; d->hecho = 0; d->estado = PENDIENTE; d->usado = 1;
    g_ndesc++;
    ui_ok("Agregada a la cola: %s -> %s", url, d->salida);
}

static void iniciar_todas(void) {
    int lanzadas = 0;
    for (int i = 0; i < g_ndesc; ++i) {
        if (g_desc[i].usado && g_desc[i].estado == PENDIENTE) {
            if (pthread_create(&g_desc[i].tid, NULL, worker, &g_desc[i]) == 0)
                lanzadas++;
        }
    }
    ui_ok("%d descarga(s) iniciada(s) en paralelo (hilos).", lanzadas);
}

static void monitor(void) {
    /* Refresca el progreso hasta que todas terminen o el usuario pulse ENTER */
    ui_info("Monitor de progreso. (Se actualiza automaticamente)");
    for (int iter = 0; iter < 600; ++iter) {
        int activas = 0;
        printf("\033[H\033[2J"); /* limpiar */
        printf(C_BOLD C_CYAN "=== Gestor de Descargas (monitor en vivo) ===\n\n" C_RESET);
        pthread_mutex_lock(&g_mtx);
        for (int i = 0; i < g_ndesc; ++i) {
            Descarga *d = &g_desc[i];
            if (!d->usado) continue;
            double pct = (d->total > 0) ? (double)d->hecho / (double)d->total * 100.0
                                        : (d->estado == COMPLETADA ? 100.0 : 0.0);
            char etiqueta[20];
            snprintf(etiqueta, sizeof(etiqueta), "#%d %s", i+1, estado_txt(d->estado));
            ui_barra_progreso(etiqueta, pct, d->hecho, d->total > 0 ? d->total : d->hecho);
            printf("  %s\n", d->url);
            if (d->estado == DESCARGANDO || d->estado == PENDIENTE) activas++;
        }
        printf("\n" C_DIM "Eventos recientes:\n" C_RESET);
        int desde = g_nev > 6 ? g_nev - 6 : 0;
        for (int i = desde; i < g_nev; ++i) printf("  %s\n", g_eventos[i]);
        pthread_mutex_unlock(&g_mtx);
        if (activas == 0) { printf("\n"); ui_ok("Todas las descargas finalizaron."); break; }
        struct timespec ts = {0, 300*1000*1000};
        nanosleep(&ts, NULL);
    }
}

static void ver_estado(void) {
    ui_encabezado("Estado de la Cola de Descargas");
    if (g_ndesc == 0) { ui_info("La cola esta vacia."); return; }
    pthread_mutex_lock(&g_mtx);
    for (int i = 0; i < g_ndesc; ++i) {
        Descarga *d = &g_desc[i];
        printf("  " C_BOLD "#%d" C_RESET " [%s] %s\n", i+1, estado_txt(d->estado), d->url);
        printf("      salida: %s | bytes: %ld", d->salida, d->hecho);
        if (d->estado == COMPLETADA) printf(" | sha256: %.16s...", d->sha);
        printf("\n");
    }
    pthread_mutex_unlock(&g_mtx);
}

static void cancelar(void) {
    long idx;
    if (util_leer_entero("Numero de descarga a cancelar: ", &idx) != 0) return;
    if (idx >= 1 && idx <= g_ndesc) {
        pthread_mutex_lock(&g_mtx); g_desc[idx-1].cancelar = 1; pthread_mutex_unlock(&g_mtx);
        ui_ok("Solicitud de cancelacion enviada a #%ld.", idx);
    } else ui_advertencia("Indice invalido.");
}

void descargas_menu(void) {
    int salir = 0;
    while (!salir) {
        ui_encabezado("Gestor de Descargas (red + hilos)");
        printf(C_DIM "  Descargas HTTP concurrentes por sockets TCP.\n\n" C_RESET);
        printf("  " C_BOLD "1)" C_RESET " Agregar URL a la cola\n");
        printf("  " C_BOLD "2)" C_RESET " Iniciar todas (en paralelo)\n");
        printf("  " C_BOLD "3)" C_RESET " Monitor de progreso en vivo\n");
        printf("  " C_BOLD "4)" C_RESET " Ver estado de la cola\n");
        printf("  " C_BOLD "5)" C_RESET " Cancelar una descarga\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        switch (op) {
            case 1: agregar(); ui_pausa(); break;
            case 2: iniciar_todas(); ui_pausa(); break;
            case 3: monitor(); ui_pausa(); break;
            case 4: ver_estado(); ui_pausa(); break;
            case 5: cancelar(); ui_pausa(); break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}
