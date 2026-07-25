#ifndef PROCESOS_H
#define PROCESOS_H

#include <sys/types.h>

#define MAX_PROC 4096

typedef struct {
    pid_t pid;
    pid_t ppid;
    char  nombre[256];
    char  estado;          /* R, S, D, Z, T ... */
    unsigned long rss_kb;  /* memoria residente */
    double mem_pct;        /* % de memoria total */
    double cpu_pct;        /* % de CPU (muestreo) */
    char  cmdline[512];
} ProcInfo;

/* Menu principal del modulo (interactivo) */
void procesos_menu(void);

#endif /* PROCESOS_H */
