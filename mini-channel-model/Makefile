# Makefile for mini-channel-model
# Wireless Channel Modeling Library

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -O2 -g
INC      := -Iinclude
AR       := ar
ARFLAGS  := rcs

SRC_DIR  := src
INC_DIR  := include
TST_DIR  := tests
EXA_DIR  := examples

CORE_SRCS := $(SRC_DIR)/channel_core.c \
             $(SRC_DIR)/pathloss.c \
             $(SRC_DIR)/fading.c \
             $(SRC_DIR)/multipath.c \
             $(SRC_DIR)/mimo_channel.c \
             $(SRC_DIR)/doppler.c

CORE_OBJS := $(CORE_SRCS:.c=.o)
LIB_NAME  := libchannelmodel.a

TEST_SRCS := $(TST_DIR)/test_channel.c
TEST_BIN  := $(TST_DIR)/test_channel

EXAMPLE_SRCS := $(EXA_DIR)/example_basic_link.c \
                $(EXA_DIR)/example_ber_fading.c \
                $(EXA_DIR)/example_mimo_capacity.c

EXAMPLE_BINS := $(EXAMPLE_SRCS:.c=)

.PHONY: all lib test examples clean lines help

all: lib examples test

lib: $(LIB_NAME)

$(LIB_NAME): $(CORE_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/channel_defs.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) $(LIB_NAME)
	$(CC) $(CFLAGS) $(INC) $< -L. -lchannelmodel -lm -o $@

examples: $(EXAMPLE_BINS)

$(EXA_DIR)/%: $(EXA_DIR)/%.c $(LIB_NAME)
	$(CC) $(CFLAGS) $(INC) $< -L. -lchannelmodel -lm -o $@

clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(TST_DIR)/*.o $(TEST_BIN)
	rm -f $(EXA_DIR)/*.o $(EXAMPLE_BINS)
	rm -f $(LIB_NAME)

lines:
	@echo "=== mini-channel-model line count ==="
	@echo -n "Headers (include/): "; find include -name "*.h" -exec cat {} + | wc -l
	@echo -n "Sources (src/): "; find src -name "*.c" -o -name "*.lean" | xargs cat | wc -l
	@echo -n "Total include/ + src/: "; cat include/*.h src/*.c src/*.lean 2>/dev/null | wc -l

help:
	@echo "mini-channel-model - Wireless Channel Modeling Library"
	@echo "Targets: make, make test, make examples, make clean, make lines"
