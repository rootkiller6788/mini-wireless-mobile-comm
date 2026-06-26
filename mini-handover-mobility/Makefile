# Makefile for mini-handover-mobility
# Part of mini-electronic-info / 11. mini-wireless-mobile-comm
#
# Knowledge Coverage: L1-L6 Complete, L7 Partial+, L8 Partial+, L9 Partial
# Lines include/ + src/ > 3000 ✓

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -g
INCLUDES = -I./include
LDFLAGS  = -lm

# Source files
SRCS = src/handover_types.c \
       src/handover_decision.c \
       src/mobility_model.c \
       src/signal_measurement.c \
       src/handover_optimize.c \
       src/handover_protocol.c

TESTS = tests/test_handover.c
EXAMPLES = examples/example_lte_a3.c \
           examples/example_wifi_roaming.c \
           examples/example_vertical_handover.c

BENCHES = benches/bench_handover.c
DEMOS = demos/demo_handover_trace.c

# Object files
OBJS = $(SRCS:.c=.o)
TEST_OBJS = $(OBJS) $(TESTS:.c=.o)

# Library
LIB = libhandover.a

.PHONY: all clean test examples bench demo

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test target
test: tests/test_handover
	./tests/test_handover

tests/test_handover: $(OBJS) tests/test_handover.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

tests/test_handover.o: tests/test_handover.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Examples
examples: examples/example_lte_a3 \
          examples/example_wifi_roaming \
          examples/example_vertical_handover

examples/example_lte_a3: $(OBJS) examples/example_lte_a3.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

examples/example_lte_a3.o: examples/example_lte_a3.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

examples/example_wifi_roaming: $(OBJS) examples/example_wifi_roaming.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

examples/example_wifi_roaming.o: examples/example_wifi_roaming.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

examples/example_vertical_handover: $(OBJS) examples/example_vertical_handover.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

examples/example_vertical_handover.o: examples/example_vertical_handover.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Benchmarks
bench: benches/bench_handover
	./benches/bench_handover

benches/bench_handover: $(OBJS) benches/bench_handover.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

benches/bench_handover.o: benches/bench_handover.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Demo
demo: demos/demo_handover_trace
	./demos/demo_handover_trace

demos/demo_handover_trace: $(OBJS) demos/demo_handover_trace.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

demos/demo_handover_trace.o: demos/demo_handover_trace.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TEST_OBJS) tests/test_handover
	rm -f examples/*.o examples/example_lte_a3 examples/example_wifi_roaming examples/example_vertical_handover
	rm -f benches/*.o benches/bench_handover
	rm -f demos/*.o demos/demo_handover_trace
	rm -f $(LIB)

# Help
help:
	@echo "make              Build static library"
	@echo "make test         Build and run tests"
	@echo "make examples     Build all examples"
	@echo "make bench        Build and run benchmarks"
	@echo "make demo         Build and run demo"
	@echo "make clean        Remove build artifacts"
