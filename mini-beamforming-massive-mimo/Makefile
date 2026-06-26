# mini-beamforming-massive-mimo Makefile
# Knowledge First · Mini-Electronic-Info

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm
AR       = ar
ARFLAGS  = rcs

SRCDIR   = src
TESTDIR  = tests
EXAMDIR  = examples
BENCHDIR = benches
BUILDDIR = build

SRCS     = src/beamforming_types.c src/beamforming_array.c src/beamforming_precoder.c src/beamforming_doa.c src/beamforming_mimo.c src/beamforming_adaptive.c
OBJS     = build/beamforming_types.o build/beamforming_array.o build/beamforming_precoder.o build/beamforming_doa.o build/beamforming_mimo.o build/beamforming_adaptive.o

EXAMPLES = build/example_ula_pattern build/example_music_doa build/example_mu_mimo build/example_massive_mimo
TESTS    = build/test_beamforming
LIBNAME  = libbeamforming_mimo.a
LIB      = build/$(LIBNAME)

.PHONY: all
all: build lib

build:
	mkdir -p build

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: lib
lib: $(LIB)

build/example_%: examples/%.c $(LIB)
	$(CC) $(CFLAGS) $< -Lbuild -lbeamforming_mimo $(LDFLAGS) -o $@

.PHONY: examples
examples: $(EXAMPLES)

build/test_%: tests/%.c $(LIB)
	$(CC) $(CFLAGS) $< -Lbuild -lbeamforming_mimo $(LDFLAGS) -o $@

.PHONY: test
test: $(TESTS)
	@echo "=== Running tests ==="
	@passed=0; failed=0; \
	for t in $(TESTS); do \
		if [ -f "$$t" ]; then \
			printf "Running %s... " "$$t"; \
			if ./$$t; then \
				echo "PASS"; passed=$$((passed+1)); \
			else \
				echo "FAIL"; failed=$$((failed+1)); \
			fi; \
		fi; \
	done; \
	echo "=== Results: $$passed passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

.PHONY: count
count:
	@echo "=== Line Count ==="
	@echo "Headers (include/):"
	@cat include/*.h | wc -l
	@echo "Source (src/*.c):"
	@cat src/*.c | wc -l

.PHONY: clean
clean:
	rm -rf build

.PHONY: check
check: clean all test count
	@echo ""
	@echo "=== CHECK COMPLETE ==="
