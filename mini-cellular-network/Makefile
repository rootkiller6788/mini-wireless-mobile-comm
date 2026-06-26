# Makefile for mini-cellular-network
# Cellular Network Architecture & Engineering Module

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -g
LDFLAGS = -lm
INC     = -Iinclude

SRC_DIR   = src
TEST_DIR  = tests
EXAMP_DIR = examples
DEMO_DIR  = demos
BENCH_DIR = benches

# All source files
SRCS = $(SRC_DIR)/cell_network_defs.c        $(SRC_DIR)/cell_network_model.c        $(SRC_DIR)/cell_network_link.c        $(SRC_DIR)/cell_network_scheduler.c        $(SRC_DIR)/cell_network_handover.c        $(SRC_DIR)/cell_network_power.c        $(SRC_DIR)/cell_network_capacity.c        $(SRC_DIR)/cell_network_deployment.c

OBJS = $(SRCS:.c=.o)

# Test
TEST_SRC = $(TEST_DIR)/test_cell_network.c
TEST_BIN = $(TEST_DIR)/test_cell_network

# Examples
EXAMPLES = $(EXAMP_DIR)/example_cell_planning            $(EXAMP_DIR)/example_scheduler            $(EXAMP_DIR)/example_link_budget

# Default target
.PHONY: all
all: test

# Compile all source files
$(OBJS): $(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Test target
.PHONY: test
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

# Examples
.PHONY: examples
examples: $(EXAMPLES)

$(EXAMP_DIR)/example_cell_planning: $(EXAMP_DIR)/example_cell_planning.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

$(EXAMP_DIR)/example_scheduler: $(EXAMP_DIR)/example_scheduler.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

$(EXAMP_DIR)/example_link_budget: $(EXAMP_DIR)/example_link_budget.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

# Demo
.PHONY: demo
demo: $(DEMO_DIR)/demo_capacity
	./$(DEMO_DIR)/demo_capacity

$(DEMO_DIR)/demo_capacity: $(DEMO_DIR)/demo_capacity.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

# Benchmark
.PHONY: bench
bench: $(BENCH_DIR)/bench_scheduler
	./$(BENCH_DIR)/bench_scheduler

$(BENCH_DIR)/bench_scheduler: $(BENCH_DIR)/bench_scheduler.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) $< $(OBJS) -o $@ $(LDFLAGS)

# Clean
.PHONY: clean
clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(TEST_BIN)
	rm -f $(EXAMPLES)
	rm -f $(DEMO_DIR)/demo_capacity
	rm -f $(BENCH_DIR)/bench_scheduler

# Line count
.PHONY: lines
lines:
	@echo "include/ lines:"
	@cat include/*.h | wc -l
	@echo "src/ lines:"
	@cat src/*.c | wc -l
	@echo "Total include/ + src/:"
	@cat include/*.h src/*.c | wc -l

# Help
.PHONY: help
help:
	@echo "mini-cellular-network Makefile targets:"
	@echo "  make          - Compile and run tests"
	@echo "  make test     - Run test suite"
	@echo "  make examples - Build all examples"
	@echo "  make demo     - Build and run demo"
	@echo "  make bench    - Build and run benchmark"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make lines    - Count source lines"
	@echo "  make help     - Show this help"
