CC      := gcc
CSTD    := -std=gnu11
WARN    := -Wall -Wextra
NOWARN  := -Wno-format-truncation -Wno-stringop-truncation
CFLAGS  := $(CSTD) $(WARN) $(NOWARN) -O2 -Iinclude -pthread
LDFLAGS := -pthread
LDLIBS  := -lz

SRC_DIR := src
OBJ_DIR := obj
BIN     := admin

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(wildcard include/*.h)

.PHONY: all run clean rebuild help

all: $(BIN)
	@echo "\n[OK] Compilacion terminada. Ejecuta:  ./$(BIN)"

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

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
