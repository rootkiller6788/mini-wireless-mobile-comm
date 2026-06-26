# Makefile for mini-lora-nbiot
# LoRa & NB-IoT LPWAN Communication Module

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm
AR       = ar

SRC = src/lora_modem.c src/lora_packet.c src/lora_linkbudget.c       src/lora_mac.c src/nbiot_phy.c src/nbiot_power.c       src/channel_model.c
OBJ = src/lora_modem.o src/lora_packet.o src/lora_linkbudget.o       src/lora_mac.o src/nbiot_phy.o src/nbiot_power.o       src/channel_model.o
LIB = liblora_nbiot.a
TEST_SRC = tests/test_lora.c
TEST_BIN = test_lora

.PHONY: all clean test examples

all: $(LIB) test

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) -L. -llora_nbiot $(LDFLAGS) -o $(TEST_BIN)
	@echo "Running tests..."
	./$(TEST_BIN)

examples: $(LIB)
	$(CC) $(CFLAGS) examples/lora_demo.c -L. -llora_nbiot $(LDFLAGS) -o lora_demo
	$(CC) $(CFLAGS) examples/nbiot_demo.c -L. -llora_nbiot $(LDFLAGS) -o nbiot_demo
	$(CC) $(CFLAGS) examples/link_budget_demo.c -L. -llora_nbiot $(LDFLAGS) -o link_budget_demo
	$(CC) $(CFLAGS) examples/app_profile_demo.c -L. -llora_nbiot $(LDFLAGS) -o app_profile_demo
	@echo "All examples built."

clean:
	rm -f src/*.o $(LIB) $(TEST_BIN) lora_demo.exe nbiot_demo.exe link_budget_demo.exe app_profile_demo.exe
