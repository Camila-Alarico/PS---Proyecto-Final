# ============================================================
#  Makefile - ADMIN en Linux
#  Programacion de Sistemas - UNSA 2026
#  Compilacion separada: cada .c -> obj/.o -> binario 'admin'
# ============================================================

CC      := gcc
CSTD    := -std=gnu11
WARN    := -Wall -Wextra
# Se silencian solo las advertencias de "peor caso" de snprintf/strncpy
# (falsos positivos: los buffers estan dimensionados con margen suficiente).
NOWARN  := -Wno-format-truncation -Wno-stringop-truncation
CFLAGS  := $(CSTD) $(WARN) $(NOWARN) -O2 -Iinclude -pthread
LDFLAGS := -pthread
LDLIBS  := -lz

SRC_DIR := src
OBJ_DIR := obj
BIN     := admin

# Descubrir automaticamente todos los fuentes
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(wildcard include/*.h)

.PHONY: all run clean rebuild help

all: $(BIN)
	@echo "\n[OK] Compilacion terminada. Ejecuta:  ./$(BIN)"

# Enlace final
$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Regla de compilacion separada (un objeto por cada fuente)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

run: all
	./$(BIN)

clean:
	@rm -rf $(OBJ_DIR) $(BIN)
	@echo "[OK] Limpieza completada."

rebuild: clean all

help:
	@echo "Targets disponibles:"
	@echo "  make        -> compila el proyecto (binario ./admin)"
	@echo "  make run    -> compila y ejecuta"
	@echo "  make clean  -> elimina objetos y binario"
	@echo "  make rebuild-> limpia y recompila desde cero"
