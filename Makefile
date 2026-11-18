CC       ?= gcc
CSTD      = -std=c11 -Wall -Wextra -pedantic
POSIX     = -D_POSIX_C_SOURCE=200809L
INCLUDES  = -Iinclude
CFLAGS    = $(CSTD) $(POSIX) $(INCLUDES) -O2
DBGFLAGS  = $(CSTD) $(POSIX) $(INCLUDES) -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer

SRC_LIB   = src/config.c
SRC_MAIN  = src/main.c
TESTS     = tests/test_config.c

BIN       = simulador
BIN_DBG   = simulador_debug
BIN_TEST  = tests/test_config

.PHONY: all debug test clean

all: $(BIN)

$(BIN): $(SRC_MAIN) $(SRC_LIB) include/config.h
	$(CC) $(CFLAGS) -o $@ $(SRC_MAIN) $(SRC_LIB)

debug: $(BIN_DBG)

$(BIN_DBG): $(SRC_MAIN) $(SRC_LIB) include/config.h
	$(CC) $(DBGFLAGS) -o $@ $(SRC_MAIN) $(SRC_LIB)

# Los tests siempre se compilan con sanitizers.
test: $(TESTS) $(SRC_LIB) include/config.h
	$(CC) $(DBGFLAGS) -o $(BIN_TEST) $(TESTS) $(SRC_LIB)
	./$(BIN_TEST)

clean:
	rm -f $(BIN) $(BIN_DBG) $(BIN_TEST)
