# PhysicsCoin Makefile v2.0

CC = gcc
CFLAGS = -O3 -Wall -Wextra -march=native -I./include
LDFLAGS = -lm

SRC_DIR = src
BUILD_DIR = build
BIN = physicscoin

# Source files (including new features)
SRCS = $(SRC_DIR)/cli/main.c \
       $(SRC_DIR)/core/state.c \
       $(SRC_DIR)/core/proofs.c \
       $(SRC_DIR)/core/streams.c \
       $(SRC_DIR)/core/batch.c \
       $(SRC_DIR)/core/replay.c \
       $(SRC_DIR)/core/timetravel.c \
       $(SRC_DIR)/crypto/crypto.c \
       $(SRC_DIR)/crypto/sha256.c \
       $(SRC_DIR)/utils/serialize.c \
       $(SRC_DIR)/utils/delta.c \
       $(SRC_DIR)/network/gossip.c \
       $(SRC_DIR)/network/sharding.c \
       $(SRC_DIR)/network/sockets.c \
       $(SRC_DIR)/consensus/vector_clock.c \
       $(SRC_DIR)/consensus/ordering.c \
       $(SRC_DIR)/consensus/checkpoint.c \
       $(SRC_DIR)/consensus/validator.c \
       $(SRC_DIR)/persistence/wal.c \
       $(SRC_DIR)/api/api.c

# Library sources (no main)
LIB_SRCS = $(SRC_DIR)/core/state.c \
           $(SRC_DIR)/core/proofs.c \
           $(SRC_DIR)/core/streams.c \
           $(SRC_DIR)/core/batch.c \
           $(SRC_DIR)/core/replay.c \
           $(SRC_DIR)/core/timetravel.c \
           $(SRC_DIR)/crypto/crypto.c \
           $(SRC_DIR)/crypto/sha256.c \
           $(SRC_DIR)/utils/serialize.c \
           $(SRC_DIR)/utils/delta.c \
           $(SRC_DIR)/network/gossip.c \
           $(SRC_DIR)/network/sharding.c \
           $(SRC_DIR)/network/sockets.c \
           $(SRC_DIR)/consensus/vector_clock.c \
           $(SRC_DIR)/consensus/ordering.c \
           $(SRC_DIR)/consensus/checkpoint.c \
           $(SRC_DIR)/consensus/validator.c \
           $(SRC_DIR)/persistence/wal.c \
           $(SRC_DIR)/api/api.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
LIB_OBJS = $(LIB_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install demo test test-all

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN) state.pcs wallet.pcw *.pcp
	rm -f test_conservation test_serialization test_performance test_exploration test_features

install: $(BIN)
	cp $(BIN) /usr/local/bin/

demo: $(BIN)
	./$(BIN) demo

stream-demo: $(BIN)
	./$(BIN) stream demo

# Build all library objects
lib: $(LIB_OBJS)

# Test targets
test_conservation: $(LIB_OBJS) tests/test_conservation.c
	$(CC) $(CFLAGS) -o $@ tests/test_conservation.c $(LIB_OBJS) $(LDFLAGS)

test_serialization: $(LIB_OBJS) tests/test_serialization.c
	$(CC) $(CFLAGS) -o $@ tests/test_serialization.c $(LIB_OBJS) $(LDFLAGS)

test_performance: $(LIB_OBJS) tests/test_performance.c
	$(CC) $(CFLAGS) -o $@ tests/test_performance.c $(LIB_OBJS) $(LDFLAGS)

test_exploration: $(LIB_OBJS) tests/test_exploration.c
	$(CC) $(CFLAGS) -o $@ tests/test_exploration.c $(LIB_OBJS) $(LDFLAGS)

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
