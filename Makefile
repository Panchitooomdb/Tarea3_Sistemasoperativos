CC       ?= gcc
CSTD      = -std=c11 -Wall -Wextra -pedantic
POSIX     = -D_POSIX_C_SOURCE=200809L
INCLUDES  = -Iinclude
CFLAGS    = $(CSTD) $(POSIX) $(INCLUDES) -O2
DBGFLAGS  = $(CSTD) $(POSIX) $(INCLUDES) -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer

# Todo src/*.c excepto main.c forma la "biblioteca" que usan el programa y las pruebas.
SRC_LIB   = $(filter-out src/main.c,$(wildcard src/*.c))
SRC_MAIN  = src/main.c
HDRS      = $(wildcard include/*.h)
TEST_SRC  = $(wildcard tests/test_*.c)
TEST_BIN  = $(TEST_SRC:.c=)

BIN       = simulador
BIN_DBG   = simulador_debug

.PHONY: all debug test clean

all: $(BIN)

$(BIN): $(SRC_MAIN) $(SRC_LIB) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC_MAIN) $(SRC_LIB)

debug: $(BIN_DBG)

$(BIN_DBG): $(SRC_MAIN) $(SRC_LIB) $(HDRS)
	$(CC) $(DBGFLAGS) -o $@ $(SRC_MAIN) $(SRC_LIB)

# Cada tests/test_X.c genera el binario tests/test_X (siempre con sanitizers).
tests/test_%: tests/test_%.c $(SRC_LIB) $(HDRS)
	$(CC) $(DBGFLAGS) -o $@ $< $(SRC_LIB)

test: $(TEST_BIN)
	@for t in $(TEST_BIN); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f $(BIN) $(BIN_DBG) $(TEST_BIN)
