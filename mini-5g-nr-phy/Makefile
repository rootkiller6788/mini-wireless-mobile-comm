# mini-5g-nr-phy Makefile
# Knowledge First · Mini-Electronic-Info

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm
AR       = ar
ARFLAGS  = rcs

SRCS = src/nr_phy_common.c src/nr_phy_ofdm.c src/nr_phy_channel.c src/nr_phy_coding.c src/nr_phy_mimo.c src/nr_phy_ssb.c src/nr_phy_pdcch.c
OBJS = build/nr_phy_common.o build/nr_phy_ofdm.o build/nr_phy_channel.o build/nr_phy_coding.o build/nr_phy_mimo.o build/nr_phy_ssb.o build/nr_phy_pdcch.o
EXAMPLES = build/example_ofdm_chain build/example_ldpc_codec build/example_ssb_detection build/example_5g_downlink
TESTS = build/test_common build/test_ofdm build/test_coding build/test_mimo build/test_ssb
LIBNAME = libnr5gphy.a
LIB     = build/$(LIBNAME)

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
	$(CC) $(CFLAGS) $< -Lbuild -lnr5gphy $(LDFLAGS) -o $@

.PHONY: examples
examples: $(EXAMPLES)

build/test_%: tests/test_%.c $(LIB)
	$(CC) $(CFLAGS) $< -Lbuild -lnr5gphy $(LDFLAGS) -o $@

.PHONY: test
test: $(TESTS)
	@echo "=== Running Tests ==="
	@passed=0; failed=0; for t in $(TESTS); do if [ -f "$$t" ]; then printf "Running %s... " "$$t"; if ./$$t > /dev/null 2>&1; then echo "PASS"; passed=$$((passed+1)); else echo "FAIL"; failed=$$((failed+1)); fi; else echo "$$t not found"; failed=$$((failed+1)); fi; done; echo "=== Results: $$passed passed, $$failed failed ==="; [ $$failed -eq 0 ]

.PHONY: count
count:
	@echo "=== Line Count ==="
	@echo "Headers (include/):"
	@cat include/*.h | wc -l
	@echo "Source (src/*.c):"
	@cat src/*.c | wc -l
	@echo "Lean (src/*.lean):"
	@cat src/*.lean | wc -l

.PHONY: clean
clean:
	rm -rf build

.PHONY: check
check: clean all test count
	@echo ""
	@echo "=== CHECK COMPLETE ==="
