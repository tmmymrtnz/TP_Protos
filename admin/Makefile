CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -MMD -fsanitize=address -I../src/include
LDFLAGS := -fsanitize=address
SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := ../bin
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
TARGET := $(BIN_DIR)/admin

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
