#include "analizador.h"
#include "ui.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Analizador estatico de scripts Bash.
 * Detecta (segun el silabo, Tema 02): variables, condicionales y ciclos.
 */

static int es_ident_ini(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int es_ident(char c)     { return isalnum((unsigned char)c) || c == '_'; }

/* Detecta una asignacion de variable: [export|local|readonly|declare] NOMBRE= */
static int detecta_asignacion(const char *s, char *nombre, size_t n) {
    while (*s == ' ' || *s == '\t') s++;
    const char *kw[] = {"export ", "local ", "readonly ", "declare "};
    for (int i = 0; i < 4; ++i) {
        size_t l = strlen(kw[i]);
        if (strncmp(s, kw[i], l) == 0) { s += l; while (*s==' '||*s=='\t') s++; }
    }
    if (!es_ident_ini(*s)) return 0;
    const char *ini = s;
    while (es_ident(*s)) s++;
    /* debe seguir '=' pero no '==' */
    if (*s == '=' && *(s+1) != '=') {
        size_t len = (size_t)(s - ini);
        if (len >= n) len = n - 1;
        memcpy(nombre, ini, len);
        nombre[len] = '\0';
        return 1;
    }
    return 0;
}

/* La linea (ya sin espacios iniciales) empieza con la palabra clave? */
static int empieza_palabra(const char *s, const char *kw) {
    while (*s==' '||*s=='\t') s++;
    size_t l = strlen(kw);
    if (strncmp(s, kw, l) != 0) return 0;
    char sig = s[l];
    return sig == ' ' || sig == '\t' || sig == '\0' || sig == '(' || sig==';';
}

static void analizar(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) { ui_error("No se pudo abrir '%s'.", ruta); return; }

    int nfor=0, nwhile=0, nuntil=0, nif=0, ncase=0, nfunc=0, ncoment=0, nlineas=0;
    int nusos=0;
    char vars[512][64]; int nvars = 0;
    char shebang[256] = "(ninguno)";

    char linea[4096];
    int primera = 1;
    while (fgets(linea, sizeof(linea), f)) {
        nlineas++;
        char *s = linea;
        while (*s==' '||*s=='\t') s++;

        if (primera) {
            primera = 0;
            if (s[0]=='#' && s[1]=='!') {
                strncpy(shebang, s, sizeof(shebang)-1); shebang[sizeof(shebang)-1]='\0';
                char *nl = strchr(shebang, '\n'); if (nl) *nl='\0';
            }
        }
        if (s[0] == '#') { ncoment++; continue; }

        /* Ciclos y condicionales */
        if (empieza_palabra(s, "for"))   nfor++;
        if (empieza_palabra(s, "while")) nwhile++;
        if (empieza_palabra(s, "until")) nuntil++;
        if (empieza_palabra(s, "if"))    nif++;
        if (empieza_palabra(s, "case"))  ncase++;
        if (empieza_palabra(s, "function")) nfunc++;

        /* funcion estilo nombre() { */
        {
            const char *p = s;
            if (es_ident_ini(*p)) {
                const char *q = p; while (es_ident(*q)) q++;
                const char *r = q; while (*r==' '||*r=='\t') r++;
                if (r[0]=='(' && r[1]==')') nfunc++;
            }
        }

        /* Asignacion de variable */
        char nombre[64];
        if (detecta_asignacion(s, nombre, sizeof(nombre))) {
            int existe = 0;
            for (int i=0;i<nvars;i++) if (!strcmp(vars[i],nombre)) { existe=1; break; }
            if (!existe && nvars < 512) { strncpy(vars[nvars],nombre,63); vars[nvars][63]='\0'; nvars++; }
        }

        /* Usos de variables: $NOMBRE o ${NOMBRE} */
        for (char *p = linea; *p; ++p) {
            if (*p == '$') {
                char c = *(p+1);
                if (c == '{' || es_ident_ini(c)) nusos++;
            }
        }
    }
    fclose(f);

    ui_linea();
    ui_info("Analisis de: %s", ruta);
    printf("  Shebang            : %s\n", shebang);
    printf("  Lineas totales     : %d\n", nlineas);
    printf("  Comentarios        : %d\n", ncoment);
    ui_linea();
    printf(C_BOLD "  CICLOS\n" C_RESET);
    printf("   for   : %d\n   while : %d\n   until : %d\n", nfor, nwhile, nuntil);
    printf(C_BOLD "  CONDICIONALES\n" C_RESET);
    printf("   if    : %d\n   case  : %d\n", nif, ncase);
    printf(C_BOLD "  FUNCIONES\n" C_RESET);
    printf("   definidas : %d\n", nfunc);
    ui_linea();
    printf(C_BOLD "  VARIABLES (%d distintas, %d usos con $)\n" C_RESET, nvars, nusos);
    printf("   ");
    for (int i = 0; i < nvars; ++i) {
        printf(C_GREEN "%s " C_RESET, vars[i]);
        if ((i+1) % 6 == 0) printf("\n   ");
    }
    printf("\n");
}

void analizador_menu(void) {
    int salir = 0;
    while (!salir) {
        ui_encabezado("Analizador de Scripts Bash");
        printf("  Detecta variables, ciclos (for/while/until),\n");
        printf("  condicionales (if/case) y funciones.\n\n");
        printf("  " C_BOLD "1)" C_RESET " Analizar un script .sh\n");
        printf("  " C_BOLD "0)" C_RESET " Volver\n\n");
        long op;
        if (util_leer_entero("Opcion: ", &op) != 0) continue;
        char a[1024];
        switch (op) {
            case 1:
                util_leer_texto("Ruta del script (ej: samples/ejemplo.sh): ", a, sizeof(a));
                if (a[0]) analizar(a);
                ui_pausa(); break;
            case 0: salir = 1; break;
            default: ui_advertencia("Opcion no valida."); ui_pausa();
        }
    }
}
