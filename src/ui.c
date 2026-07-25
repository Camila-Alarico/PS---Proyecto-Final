#include "ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void ui_limpiar(void) {
    /* Secuencia ANSI: limpiar pantalla y mover cursor al inicio */
    printf("\033[2J\033[H");
}

void ui_linea(void) {
    printf(C_DIM "------------------------------------------------------------\n" C_RESET);
}

void ui_encabezado(const char *titulo) {
    ui_limpiar();
    printf(C_BOLD C_CYAN "============================================================\n");
    printf("   Zetch  " C_DIM "|" C_RESET C_BOLD C_CYAN "  %s\n", titulo);
    printf("============================================================\n" C_RESET);
}

void ui_pausa(void) {
    printf(C_DIM "\nPresiona ENTER para continuar..." C_RESET);
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { }
}

static void con_color(const char *color, const char *icono, const char *fmt, va_list ap) {
    printf("%s%s ", color, icono);
    vprintf(fmt, ap);
    printf("%s\n", C_RESET);
}

void ui_ok(const char *fmt, ...)          { va_list a; va_start(a,fmt); con_color(C_GREEN,  "[OK] ", fmt, a); va_end(a); }
void ui_error(const char *fmt, ...)       { va_list a; va_start(a,fmt); con_color(C_RED,    "[ERR]", fmt, a); va_end(a); }
void ui_info(const char *fmt, ...)        { va_list a; va_start(a,fmt); con_color(C_CYAN,   "[i]  ", fmt, a); va_end(a); }
void ui_advertencia(const char *fmt, ...) { va_list a; va_start(a,fmt); con_color(C_YELLOW, "[!]  ", fmt, a); va_end(a); }

void ui_barra_progreso(const char *etiqueta, double porcentaje, long hecho, long total) {
    int ancho = 30;
    if (porcentaje < 0)   porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;
    int llenos = (int)(porcentaje / 100.0 * ancho);
    printf("\r%-18.18s [", etiqueta);
    for (int i = 0; i < ancho; ++i) putchar(i < llenos ? '#' : '.');
    printf("] %5.1f%% (%ld/%ld)   ", porcentaje, hecho, total);
    fflush(stdout);
}
