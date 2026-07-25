#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <sys/types.h>

/* Lectura segura de entrada estandar */
int  util_leer_linea(char *buf, size_t n);
int  util_leer_entero(const char *prompt, long *out);
void util_leer_texto(const char *prompt, char *buf, size_t n);

/* Formateo de tamanos legibles (KB/MB/GB) */
void util_tam_legible(unsigned long bytes, char *out, size_t n);

/* Permisos estilo ls -l (ej: -rwxr-xr-x) */
void util_modo_texto(mode_t modo, char out[11]);

/* SHA-256 (implementacion propia) */
void util_sha256(const unsigned char *data, size_t len, char out_hex[65]);
int  util_sha256_archivo(const char *ruta, char out_hex[65]);

/* Utilidades de rutas/archivos */
int  util_es_directorio(const char *ruta);
int  util_existe(const char *ruta);
int  util_crear_dirs(const char *ruta);   /* mkdir -p */
int  util_copiar_archivo(const char *src, const char *dst); /* I/O de bajo nivel */

#endif /* UTIL_H */
