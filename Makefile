# PhysicsCoin Makefile

CC = gcc
CFLAGS = -O3 -Wall -Wextra -march=native -I./include
LDFLAGS = -lm

SRC_DIR = src
BUILD_DIR = build
BIN = physicscoin

# Source files
SRCS = $(SRC_DIR)/cli/main.c \
       $(SRC_DIR)/core/state.c \
       $(SRC_DIR)/crypto/crypto.c \
       $(SRC_DIR)/crypto/sha256.c \
       $(SRC_DIR)/utils/serialize.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install test demo

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN) state.pcs wallet.pcw

install: $(BIN)
	cp $(BIN) /usr/local/bin/

test: $(BIN)
	./$(BIN) demo

demo: $(BIN)
	./$(BIN) demo

debug: CFLAGS = -g -Wall -Wextra -I./include
debug: clean all
