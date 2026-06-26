# Makefile for mini-wifi-bluetooth
# WiFi & Bluetooth module — IEEE 802.11 + Bluetooth BR/EDR + BLE
# Built to SKILL.md standard — knowledge-first, code-as-carrier

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -O2 -g -I./include
LDFLAGS  = -lm

# Source files (C only, no Lean here — Lean compiled separately)
SRC_DIR  = src
INC_DIR  = include
TST_DIR  = tests
EXA_DIR  = examples

SRCS     = $(SRC_DIR)/wifi_bt_core.c \
           $(SRC_DIR)/wifi_phy.c \
           $(SRC_DIR)/wifi_coding.c \
           $(SRC_DIR)/wifi_mac.c \
           $(SRC_DIR)/bluetooth_core.c \
           $(SRC_DIR)/bluetooth_ble.c \
           $(SRC_DIR)/wireless_security.c

OBJS     = $(SRCS:.c=.o)

# Test target
TEST_SRC = $(TST_DIR)/test_wifi_bluetooth.c
TEST_BIN = $(TST_DIR)/test_wifi_bluetooth

# Example targets
EX1_SRC  = $(EXA_DIR)/example_wifi_ofdm.c
EX1_BIN  = $(EXA_DIR)/example_wifi_ofdm
EX2_SRC  = $(EXA_DIR)/example_ble_health_monitor.c
EX2_BIN  = $(EXA_DIR)/example_ble_health_monitor
EX3_SRC  = $(EXA_DIR)/example_wifi_wpa3.c
EX3_BIN  = $(EXA_DIR)/example_wifi_wpa3

.PHONY: all test examples clean

all: test examples

# Build test binary
$(TEST_BIN): $(SRCS) $(TEST_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)

# Build example 1: WiFi OFDM
$(EX1_BIN): $(SRCS) $(EX1_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(EX1_SRC) -o $(EX1_BIN) $(LDFLAGS)

# Build example 2: BLE Health Monitor
$(EX2_BIN): $(SRCS) $(EX2_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(EX2_SRC) -o $(EX2_BIN) $(LDFLAGS)

# Build example 3: WiFi WPA3
$(EX3_BIN): $(SRCS) $(EX3_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(EX3_SRC) -o $(EX3_BIN) $(LDFLAGS)

# Run tests
test: $(TEST_BIN)
	@echo "=== Running WiFi & Bluetooth test suite ==="
	@./$(TEST_BIN)
	@echo ""
	@echo "=== Tests complete ==="

# Build all examples
examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)
	@echo "=== All examples built ==="

# Clean build artifacts
clean:
	rm -f $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)
	rm -f $(SRC_DIR)/*.o
	@echo "=== Clean complete ==="
