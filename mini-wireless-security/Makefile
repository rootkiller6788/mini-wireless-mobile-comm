# mini-wireless-security Makefile
# Knowledge First · Mini-Electronic-Info

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm
AR       = ar
ARFLAGS  = rcs

SRCS     = src/wireless_crypto.c src/wireless_auth.c src/wireless_key_mgmt.c src/wireless_protocol.c src/wireless_attack.c src/wireless_phy_sec.c
OBJS     = build/wireless_crypto.o build/wireless_auth.o build/wireless_key_mgmt.o build/wireless_protocol.o build/wireless_attack.o build/wireless_phy_sec.o

EXAMPLES = build/example_wpa2_handshake build/example_aes_ccmp build/example_key_derivation build/example_secrecy_capacity build/example_attack_demo
TESTS    = build/test_crypto build/test_auth build/test_key_mgmt build/test_protocol build/test_attack
LIB      = build/libwireless_security.a

.PHONY: all
all: $(LIB)

.PHONY: lib
lib: $(LIB)

build:
	mkdir -p build

$(LIB): build $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/example_%: examples/example_%.c $(LIB)
	$(CC) $(CFLAGS) $< -Lbuild -lwireless_security $(LDFLAGS) -o $@

.PHONY: examples
examples: $(LIB) $(EXAMPLES)

build/test_%: tests/test_%.c $(LIB)
	$(CC) $(CFLAGS) $< -Lbuild -lwireless_security $(LDFLAGS) -o $@

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
	@echo "Lean (src/*.lean):"
	@cat src/*.lean | wc -l

.PHONY: clean
clean:
	rm -rf build

.PHONY: check
check: clean all test count
	@echo ""
	@echo "=== CHECK COMPLETE ==="
