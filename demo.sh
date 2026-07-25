#!/bin/bash
# ============================================================
# demo.sh - Prepara un entorno de demostracion para ADMIN en Linux
#   - Compila el proyecto
#   - Crea datos de ejemplo
#   - Levanta un servidor HTTP local con archivos para descargar
# ============================================================
set -e

AZUL="\033[1;36m"; VERDE="\033[1;32m"; RESET="\033[0m"

echo -e "${AZUL}== 1) Compilando el proyecto ==${RESET}"
make

echo -e "${AZUL}== 2) Preparando datos de ejemplo para descargas ==${RESET}"
mkdir -p demo_web
head -c 500000 /dev/urandom > demo_web/archivo_grande.bin 2>/dev/null || \
    dd if=/dev/urandom of=demo_web/archivo_grande.bin bs=1024 count=500 2>/dev/null
echo "Archivo de texto de ejemplo para descargar por HTTP." > demo_web/lista.txt

echo -e "${AZUL}== 3) Levantando servidor HTTP local en el puerto 8099 ==${RESET}"
( cd demo_web && python3 -m http.server 8099 >/tmp/admin_demo_http.log 2>&1 & echo $! > /tmp/admin_demo_http.pid )
sleep 1

echo -e "${VERDE}Listo!${RESET}"
echo ""
echo "Ahora ejecuta:  ./admin"
echo ""
echo "Sugerencias de demo:"
echo "  - Menu 5 (Analizador):  samples/ejemplo.sh"
echo "  - Menu 4 (Backups):     origen 'samples/datos', luego modificar y restaurar"
echo "  - Menu 6 (Descargas):   http://127.0.0.1:8099/archivo_grande.bin"
echo "                          http://127.0.0.1:8099/lista.txt"
echo ""
echo "Para detener el servidor HTTP de demo:"
echo "  kill \$(cat /tmp/admin_demo_http.pid)"
