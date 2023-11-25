# src/Makefile

CC ?= gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -MMD -fsanitize=address -Iinclude
LDFLAGS := -lpthread -fsanitize=address
SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := ../bin # Place the executable in the common bin directory
TARGET := ../pop3d
SRCS := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c) # Add subdirectories to the search path
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "Linking objects: $(OBJS) to create executable: $@"
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
