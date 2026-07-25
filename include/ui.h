#ifndef UI_H
#define UI_H

#include <stddef.h>

/* Codigos de color ANSI */
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"
#define C_BG_BLUE "\033[44m"

void ui_limpiar(void);
void ui_encabezado(const char *titulo);
void ui_linea(void);
void ui_pausa(void);
void ui_ok(const char *fmt, ...);
void ui_error(const char *fmt, ...);
void ui_info(const char *fmt, ...);
void ui_advertencia(const char *fmt, ...);
void ui_barra_progreso(const char *etiqueta, double porcentaje, long hecho, long total);

#endif /* UI_H */
