#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ---------------- Entrada segura ---------------- */

int util_leer_linea(char *buf, size_t n) {
    if (!fgets(buf, (int)n, stdin)) {
        buf[0] = '\0';
        return -1;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return 0;
}

int util_leer_entero(const char *prompt, long *out) {
    char buf[128];
    if (prompt) { printf("%s", prompt); fflush(stdout); }
    if (util_leer_linea(buf, sizeof(buf)) != 0) return -1;
    if (buf[0] == '\0') return -1;
    char *fin = NULL;
    errno = 0;
    long v = strtol(buf, &fin, 10);
    if (errno != 0 || fin == buf) return -1;
    *out = v;
    return 0;
}

void util_leer_texto(const char *prompt, char *buf, size_t n) {
    if (prompt) { printf("%s", prompt); fflush(stdout); }
    if (util_leer_linea(buf, n) != 0) buf[0] = '\0';
}

/* ---------------- Formateo ---------------- */

void util_tam_legible(unsigned long bytes, char *out, size_t n) {
    const char *u[] = {"B", "KB", "MB", "GB", "TB"};
    double b = (double)bytes;
    int i = 0;
    while (b >= 1024.0 && i < 4) { b /= 1024.0; i++; }
    if (i == 0) snprintf(out, n, "%lu %s", bytes, u[i]);
    else        snprintf(out, n, "%.1f %s", b, u[i]);
}

void util_modo_texto(mode_t m, char out[11]) {
    out[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' :
             S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';
    out[1] = (m & S_IRUSR) ? 'r' : '-';
    out[2] = (m & S_IWUSR) ? 'w' : '-';
    out[3] = (m & S_IXUSR) ? 'x' : '-';
    out[4] = (m & S_IRGRP) ? 'r' : '-';
    out[5] = (m & S_IWGRP) ? 'w' : '-';
    out[6] = (m & S_IXGRP) ? 'x' : '-';
    out[7] = (m & S_IROTH) ? 'r' : '-';
    out[8] = (m & S_IWOTH) ? 'w' : '-';
    out[9] = (m & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

/* ---------------- SHA-256 ---------------- */

typedef struct {
    unsigned int  state[8];
    unsigned long long bitlen;
    unsigned char data[64];
    unsigned int  datalen;
} sha256_ctx;

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SIG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ ((x) >> 10))

static const unsigned int K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(sha256_ctx *c, const unsigned char *d) {
    unsigned int a,b,cc,dd,e,f,g,h,t1,t2,m[64];
    unsigned int i,j;
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (d[j] << 24) | (d[j+1] << 16) | (d[j+2] << 8) | (d[j+3]);
    for (; i < 64; ++i)
        m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];
    a=c->state[0]; b=c->state[1]; cc=c->state[2]; dd=c->state[3];
    e=c->state[4]; f=c->state[5]; g=c->state[6]; h=c->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + K256[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,cc);
        h=g; g=f; f=e; e=dd+t1; dd=cc; cc=b; b=a; a=t1+t2;
    }
    c->state[0]+=a; c->state[1]+=b; c->state[2]+=cc; c->state[3]+=dd;
    c->state[4]+=e; c->state[5]+=f; c->state[6]+=g; c->state[7]+=h;
}

static void sha256_init(sha256_ctx *c) {
    c->datalen = 0; c->bitlen = 0;
    c->state[0]=0x6a09e667; c->state[1]=0xbb67ae85; c->state[2]=0x3c6ef372;
    c->state[3]=0xa54ff53a; c->state[4]=0x510e527f; c->state[5]=0x9b05688c;
    c->state[6]=0x1f83d9ab; c->state[7]=0x5be0cd19;
}

static void sha256_update(sha256_ctx *c, const unsigned char *d, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        c->data[c->datalen++] = d[i];
        if (c->datalen == 64) {
            sha256_transform(c, c->data);
            c->bitlen += 512;
            c->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char *hash) {
    unsigned int i = c->datalen;
    c->data[i++] = 0x80;
    if (c->datalen < 56) {
        while (i < 56) c->data[i++] = 0x00;
    } else {
        while (i < 64) c->data[i++] = 0x00;
        sha256_transform(c, c->data);
        memset(c->data, 0, 56);
    }
    c->bitlen += (unsigned long long)c->datalen * 8;
    for (int k = 7; k >= 0; --k)
        c->data[56 + (7 - k)] = (unsigned char)(c->bitlen >> (k * 8));
    sha256_transform(c, c->data);
    for (i = 0; i < 4; ++i)
        for (int j = 0; j < 8; ++j)
            hash[i + j*4] = (unsigned char)((c->state[j] >> (24 - i*8)) & 0xff);
}

static void hex(const unsigned char *in, int n, char *out) {
    const char *h = "0123456789abcdef";
    for (int i = 0; i < n; ++i) { out[i*2]=h[in[i]>>4]; out[i*2+1]=h[in[i]&0xf]; }
    out[n*2] = '\0';
}

void util_sha256(const unsigned char *data, size_t len, char out_hex[65]) {
    sha256_ctx c; unsigned char h[32];
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, h);
    hex(h, 32, out_hex);
}

int util_sha256_archivo(const char *ruta, char out_hex[65]) {
    int fd = open(ruta, O_RDONLY);
    if (fd < 0) return -1;
    sha256_ctx c; sha256_init(&c);
    unsigned char buf[65536];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0)
        sha256_update(&c, buf, (size_t)r);
    close(fd);
    if (r < 0) return -1;
    unsigned char h[32];
    sha256_final(&c, h);
    hex(h, 32, out_hex);
    return 0;
}

/* ---------------- Rutas / archivos ---------------- */

int util_es_directorio(const char *ruta) {
    struct stat st;
    if (stat(ruta, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int util_existe(const char *ruta) {
    struct stat st;
    return stat(ruta, &st) == 0;
}

int util_crear_dirs(const char *ruta) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", ruta);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len-1] == '/') tmp[len-1] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int util_copiar_archivo(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    struct stat st;
    fstat(in, &st);
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (out < 0) { close(in); return -1; }
    char buf[65536];
    ssize_t r;
    int rc = 0;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        ssize_t escrito = 0;
        while (escrito < r) {
            ssize_t w = write(out, buf + escrito, (size_t)(r - escrito));
            if (w < 0) { rc = -1; goto fin; }
            escrito += w;
        }
    }
    if (r < 0) rc = -1;
fin:
    close(in);
    close(out);
    return rc;
}
