#include "ui.h"
#include "util.h"
#include "procesos.h"
#include "archivos.h"
#include "comandos.h"
#include "backup.h"
#include "analizador.h"
#include "descargas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Utilidad: calcular SHA-256 de un archivo desde el menu */
static void util_checksum(void) {
    ui_encabezado("Utilidad: Checksum SHA-256");
    char ruta[1024];
    util_leer_texto("Ruta del archivo: ", ruta, sizeof(ruta));
    if (!ruta[0]) return;
    char h[65];
    if (util_sha256_archivo(ruta, h) == 0)
        ui_ok("SHA-256: %s", h);
    else
        ui_error("No se pudo leer el archivo.");
    ui_pausa();
}

static void banner(void) {
    ui_limpiar();
    printf(C_BOLD C_CYAN);
    printf("Zetch\n");
    printf(C_RESET);
    printf(C_DIM "Proyecto de Programacion de Sistemas \n\n" C_RESET);
}

int main(void) {
    /* Evitar que un socket cerrado mate el proceso (descargas) */
    signal(SIGPIPE, SIG_IGN);

    int salir = 0;
    while (!salir) {
        ui_limpiar();
        banner();
        printf(C_BOLD "  MENU PRINCIPAL\n\n" C_RESET);
        printf("  " C_BOLD C_GREEN "1)" C_RESET " Administrador de Tareas   " C_DIM "(procesos, senales, arbol)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "2)" C_RESET " Shell de Archivos         " C_DIM "(ls, cp, mv, rm, buscar, stats)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "3)" C_RESET " Comandos Linux            " C_DIM "(ejecutar, stdout/stderr, historial)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "4)" C_RESET " Backups Incrementales     " C_DIM "(snapshots, restaurar, gzip)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "5)" C_RESET " Analizador de Scripts Bash" C_DIM " (variables, ciclos)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "6)" C_RESET " Gestor de Descargas       " C_DIM "(HTTP, sockets, hilos)\n" C_RESET);
        printf("  " C_BOLD C_GREEN "7)" C_RESET " Utilidad: Checksum SHA-256\n" C_RESET);
        printf("  " C_BOLD C_RED   "0)" C_RESET " Salir\n\n");
        long op;
        if (util_leer_entero("  Selecciona una opcion: ", &op) != 0) continue;
        switch (op) {
            case 1: procesos_menu();   break;
            case 2: archivos_menu();   break;
            case 3: comandos_menu();   break;
            case 4: backup_menu();     break;
            case 5: analizador_menu(); break;
            case 6: descargas_menu();  break;
            case 7: util_checksum();   break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
    ui_limpiar();
    printf(C_BOLD C_CYAN "Gracias por usar ADMIN en Linux. Hasta pronto!\n" C_RESET);
    return 0;
}
