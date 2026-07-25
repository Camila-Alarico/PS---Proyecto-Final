#!/bin/bash
# ============================================================
# ejemplo.sh - Script de demostracion para el analizador
# Contiene variables, condicionales y ciclos (Tema 02 del silabo)
# ============================================================

# Variables
NOMBRE="ADMIN en Linux"
VERSION=1.0
export RUTA_BASE="/tmp/demo"
local contador=0
readonly MAX=10

# Funcion de saludo
saludar() {
    echo "Hola desde $NOMBRE version $VERSION"
}

# Condicional if
if [ -d "$RUTA_BASE" ]; then
    echo "El directorio $RUTA_BASE existe"
else
    echo "Creando $RUTA_BASE"
    mkdir -p "$RUTA_BASE"
fi

# Ciclo for
for i in 1 2 3 4 5; do
    echo "Iteracion for: $i"
done

# Ciclo while
while [ $contador -lt $MAX ]; do
    echo "Contador while: $contador"
    contador=$((contador + 1))
done

# Ciclo until
until [ $contador -ge 15 ]; do
    contador=$((contador + 1))
done

# Condicional case
case "$1" in
    inicio) saludar ;;
    fin)    echo "Terminando" ;;
    *)      echo "Uso: $0 {inicio|fin}" ;;
esac
