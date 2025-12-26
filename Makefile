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

# Library sources (no main)
LIB_SRCS = $(SRC_DIR)/core/state.c \
           $(SRC_DIR)/crypto/crypto.c \
           $(SRC_DIR)/crypto/sha256.c \
           $(SRC_DIR)/utils/serialize.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
LIB_OBJS = $(LIB_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install test demo test-all test-conservation test-serialization test-performance

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN) state.pcs wallet.pcw
	rm -f test_conservation test_serialization test_performance

install: $(BIN)
	cp $(BIN) /usr/local/bin/

demo: $(BIN)
	./$(BIN) demo

# Test targets
test_conservation: $(LIB_OBJS) tests/test_conservation.c
	$(CC) $(CFLAGS) -o $@ tests/test_conservation.c $(LIB_OBJS) $(LDFLAGS)

test_serialization: $(LIB_OBJS) tests/test_serialization.c
	$(CC) $(CFLAGS) -o $@ tests/test_serialization.c $(LIB_OBJS) $(LDFLAGS)

test_performance: $(LIB_OBJS) tests/test_performance.c
	$(CC) $(CFLAGS) -o $@ tests/test_performance.c $(LIB_OBJS) $(LDFLAGS)

test-conservation: test_conservation
	./test_conservation

test-serialization: test_serialization
	./test_serialization

test-performance: test_performance
	./test_performance

test-all: test_conservation test_serialization test_performance
	@echo ""
	@echo "╔═══════════════════════════════════════════════════════════════╗"
	@echo "║              RUNNING ALL PHYSICSCOIN TESTS                    ║"
	@echo "╚═══════════════════════════════════════════════════════════════╝"
	@echo ""
	./test_conservation
	./test_serialization
	./test_performance

test: test-all

debug: CFLAGS = -g -Wall -Wextra -I./include
debug: clean all
