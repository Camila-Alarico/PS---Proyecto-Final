#include "procesos.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

/* Lee el total de jiffies de CPU desde /proc/stat (primera linea) */
static unsigned long long total_cpu_jiffies(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char linea[512];
    unsigned long long total = 0;
    if (fgets(linea, sizeof(linea), f)) {
        /* cpu  user nice system idle iowait irq softirq ... */
        char *p = linea + 3;
        unsigned long long v;
        while (sscanf(p, "%llu", &v) == 1) {
            total += v;
            /* avanzar al siguiente numero */
            while (*p == ' ') p++;
            while (*p && *p != ' ') p++;
        }
    }
    fclose(f);
    return total;
}

static unsigned long mem_total_kb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    unsigned long kb = 0;
    char etiqueta[64];
    if (fscanf(f, "%63s %lu", etiqueta, &kb) != 2) kb = 0;
    fclose(f);
    return kb;
}

/* Lee utime+stime (campos 14 y 15) de /proc/pid/stat, y nombre, estado, ppid */
static int leer_stat(pid_t pid, char *nombre, char *estado, pid_t *ppid,
                     unsigned long long *proc_jiffies) {
    char ruta[64];
    snprintf(ruta, sizeof(ruta), "/proc/%d/stat", pid);
    FILE *f = fopen(ruta, "r");
    if (!f) return -1;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);

    /* El nombre esta entre parentesis y puede contener espacios */
    char *ini = strchr(buf, '(');
    char *fin = strrchr(buf, ')');
    if (!ini || !fin || fin < ini) return -1;
    size_t nlen = (size_t)(fin - ini - 1);
    if (nlen > 254) nlen = 254;
    memcpy(nombre, ini + 1, nlen);
    nombre[nlen] = '\0';

    /* Campos despues de ')': estado ppid ... */
    char *p = fin + 2;
    char st;
    int ppid_i = 0;
    /* campo 3 = estado, campo 4 = ppid */
    if (sscanf(p, "%c %d", &st, &ppid_i) < 2) return -1;
    *estado = st;
    *ppid = (pid_t)ppid_i;

    /* Saltar hasta utime(14) stime(15). Contamos campos desde estado=3 */
    int campo = 3;
    unsigned long long utime = 0, stime = 0;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *ini_num = p;
        while (*p && *p != ' ') p++;
        campo++;
        if (campo == 14) utime = strtoull(ini_num, NULL, 10);
        else if (campo == 15) { stime = strtoull(ini_num, NULL, 10); break; }
    }
    *proc_jiffies = utime + stime;
    return 0;
}

static unsigned long leer_rss_kb(pid_t pid) {
    char ruta[64];
    snprintf(ruta, sizeof(ruta), "/proc/%d/status", pid);
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;
    char linea[256];
    unsigned long rss = 0;
    while (fgets(linea, sizeof(linea), f)) {
        if (strncmp(linea, "VmRSS:", 6) == 0) {
            sscanf(linea + 6, "%lu", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

static void leer_cmdline(pid_t pid, char *out, size_t n) {
    char ruta[64];
    snprintf(ruta, sizeof(ruta), "/proc/%d/cmdline", pid);
    int fd = open(ruta, O_RDONLY);
    if (fd < 0) { out[0] = '\0'; return; }
    ssize_t r = read(fd, out, n - 1);
    close(fd);
    if (r <= 0) { out[0] = '\0'; return; }
    /* cmdline separa argumentos con \0 -> convertir a espacios */
    for (ssize_t i = 0; i < r; ++i)
        if (out[i] == '\0') out[i] = ' ';
    out[r] = '\0';
}

/* Toma una foto de PIDs con sus jiffies actuales */
static int foto_jiffies(pid_t *pids, unsigned long long *jiffies, int max) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        pid_t pid = (pid_t)atoi(e->d_name);
        char nombre[256], estado; pid_t ppid; unsigned long long pj;
        if (leer_stat(pid, nombre, &estado, &ppid, &pj) == 0) {
            pids[n] = pid;
            jiffies[n] = pj;
            n++;
        }
    }
    closedir(d);
    return n;
}

/* Recolecta la lista completa de procesos con CPU% (muestreo) y memoria */
static int recolectar(ProcInfo *lista, int max) {
    static pid_t   pids1[MAX_PROC];
    static unsigned long long j1[MAX_PROC];
    unsigned long long cpu1 = total_cpu_jiffies();
    int n1 = foto_jiffies(pids1, j1, MAX_PROC);

    /* Intervalo corto de muestreo para calcular %CPU */
    struct timespec ts = { 0, 250 * 1000 * 1000 }; /* 250 ms */
    nanosleep(&ts, NULL);

    unsigned long long cpu2 = total_cpu_jiffies();
    unsigned long long delta_cpu = (cpu2 > cpu1) ? (cpu2 - cpu1) : 1;
    unsigned long memtot = mem_total_kb();
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 1) ncpu = 1;

    int n = 0;
    for (int i = 0; i < n1 && n < max; ++i) {
        pid_t pid = pids1[i];
        char nombre[256], estado; pid_t ppid; unsigned long long pj2;
        if (leer_stat(pid, nombre, &estado, &ppid, &pj2) != 0) continue;
        unsigned long long dproc = (pj2 > j1[i]) ? (pj2 - j1[i]) : 0;
        double cpu = (double)dproc / (double)delta_cpu * 100.0 * (double)ncpu;
        if (cpu < 0) cpu = 0;
        ProcInfo *p = &lista[n++];
        p->pid = pid;
        p->ppid = ppid;
        p->estado = estado;
        strncpy(p->nombre, nombre, sizeof(p->nombre)-1);
        p->nombre[sizeof(p->nombre)-1] = '\0';
        p->rss_kb = leer_rss_kb(pid);
        p->mem_pct = memtot ? (double)p->rss_kb / (double)memtot * 100.0 : 0.0;
        p->cpu_pct = cpu;
        leer_cmdline(pid, p->cmdline, sizeof(p->cmdline));
    }
    return n;
}

static int cmp_cpu(const void *a, const void *b) {
    double d = ((const ProcInfo*)b)->cpu_pct - ((const ProcInfo*)a)->cpu_pct;
    return (d > 0) - (d < 0);
}

static void imprimir_tabla(ProcInfo *l, int n, const char *filtro) {
    printf(C_BOLD "%-8s %-8s %-6s %6s %8s  %-24s\n" C_RESET,
           "PID", "PPID", "ESTADO", "CPU%", "MEM", "NOMBRE");
    ui_linea();
    int mostrados = 0;
    for (int i = 0; i < n; ++i) {
        if (filtro && filtro[0] && !strstr(l[i].nombre, filtro)) continue;
        char mem[32];
        util_tam_legible(l[i].rss_kb * 1024UL, mem, sizeof(mem));
        const char *color = l[i].cpu_pct > 10.0 ? C_YELLOW :
                            (l[i].estado == 'Z' ? C_RED : C_RESET);
        printf("%s%-8d %-8d %-6c %6.1f %8s  %-24.24s\n" C_RESET,
               color, l[i].pid, l[i].ppid, l[i].estado,
               l[i].cpu_pct, mem, l[i].nombre);
        mostrados++;
    }
    ui_linea();
    ui_info("%d proceso(s) mostrado(s).", mostrados);
}

static void accion_listar(const char *filtro) {
    ui_encabezado(filtro && filtro[0] ? "Procesos (busqueda)" : "Lista de Procesos");
    ui_info("Muestreando CPU (250 ms)...");
    static ProcInfo lista[MAX_PROC];
    int n = recolectar(lista, MAX_PROC);
    qsort(lista, n, sizeof(ProcInfo), cmp_cpu);
    ui_limpiar();
    ui_encabezado(filtro && filtro[0] ? "Procesos (busqueda)" : "Lista de Procesos");
    imprimir_tabla(lista, n, filtro);
}

static void accion_senal(int sig, const char *desc) {
    long pid;
    if (util_leer_entero("PID objetivo: ", &pid) != 0) {
        ui_error("PID invalido.");
        return;
    }
    if (kill((pid_t)pid, sig) == 0)
        ui_ok("Senal %s enviada al PID %ld.", desc, pid);
    else
        ui_error("No se pudo enviar la senal (%s). PID=%ld", strerror(errno), pid);
}

/* Arbol de procesos: imprime recursivamente por PPid */
static void imprimir_arbol(ProcInfo *l, int n, pid_t raiz, int nivel) {
    for (int i = 0; i < n; ++i) {
        if (l[i].ppid == raiz) {
            for (int k = 0; k < nivel; ++k) printf("  ");
            printf(C_GREEN "|- " C_RESET "%s" C_DIM " (%d)\n" C_RESET,
                   l[i].nombre, l[i].pid);
            if (nivel < 40)
                imprimir_arbol(l, n, l[i].pid, nivel + 1);
        }
    }
}

static void accion_arbol(void) {
    ui_encabezado("Arbol de Procesos");
    static ProcInfo lista[MAX_PROC];
    int n = recolectar(lista, MAX_PROC);
    long raiz = 1;
    printf("PID raiz (ENTER=1): ");
    char buf[32];
    util_leer_linea(buf, sizeof(buf));
    if (buf[0]) raiz = atol(buf);
    ui_limpiar();
    ui_encabezado("Arbol de Procesos");
    printf(C_BOLD "Raiz: PID %ld\n\n" C_RESET, raiz);
    imprimir_arbol(lista, n, (pid_t)raiz, 0);
}

void procesos_menu(void) {
    int salir = 0;
    while (!salir) {
        ui_encabezado("Administrador de Tareas");
        printf("  " C_BOLD "1)" C_RESET " Listar procesos (ordenado por CPU)\n");
        printf("  " C_BOLD "2)" C_RESET " Buscar proceso por nombre\n");
        printf("  " C_BOLD "3)" C_RESET " Terminar proceso (SIGTERM)\n");
        printf("  " C_BOLD "4)" C_RESET " Forzar terminacion (SIGKILL)\n");
        printf("  " C_BOLD "5)" C_RESET " Suspender proceso (SIGSTOP)\n");
        printf("  " C_BOLD "6)" C_RESET " Reanudar proceso (SIGCONT)\n");
        printf("  " C_BOLD "7)" C_RESET " Ver arbol de procesos\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        switch (op) {
            case 1: accion_listar(NULL); ui_pausa(); break;
            case 2: {
                char f[128];
                util_leer_texto("Nombre a buscar: ", f, sizeof(f));
                accion_listar(f); ui_pausa(); break;
            }
            case 3: accion_senal(SIGTERM, "SIGTERM"); ui_pausa(); break;
            case 4: accion_senal(SIGKILL, "SIGKILL"); ui_pausa(); break;
            case 5: accion_senal(SIGSTOP, "SIGSTOP"); ui_pausa(); break;
            case 6: accion_senal(SIGCONT, "SIGCONT"); ui_pausa(); break;
            case 7: accion_arbol(); ui_pausa(); break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}
