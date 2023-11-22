TARGET := main
DEBUG := y
ASAN := y

# Default to gcc, can be overridden by calling `make CC=clang`
CC ?= gcc
PACKAGES := check

TARGET_SOURCES := $(shell find src/ -name '*.c')
TARGET_OBJECTS := $(TARGET_SOURCES:%.c=%.o)
TARGET_OBJECTS_MINUS_MAIN := $(filter-out src/main.o, $(TARGET_OBJECTS))

TEST_SOURCES := $(shell find test/ -name '*.c')
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

INCLUDES := -Iinclude -Isrc -Isrc/include $(shell pkg-config --cflags $(PACKAGES))
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -MMD $(INCLUDES)
LDFLAGS := -lpthread $(shell pkg-config --libs $(PACKAGES))
TEST_CFLAGS := $(filter-out -Werror, $(CFLAGS))

ifeq ($(DEBUG), y)
	CFLAGS += -ggdb3 -O0
endif
ifeq ($(ASAN), y)
	CFLAGS += -fsanitize=address
	LDFLAGS += -fsanitize=address
endif
ifneq ($(and $(DEBUG),$(ASAN)),y)
	CFLAGS += -O3
endif

.PHONY: clean test

# Link target
$(TARGET): $(TARGET_OBJECTS)
	$(CC) $(CFLAGS) $(TARGET_OBJECTS) -o $@ $(LDFLAGS)

test/%_test.o: test/%_test.c
	$(CC) $(TEST_CFLAGS) -c $< -o $@

test: $(TEST_OBJECTS) $(TARGET_OBJECTS_MINUS_MAIN)

clean:
	rm -f **/*.o **/*.d
	rm -f $(TARGET) *_test

-include $(TARGET_OBJECTS:%.o=%.d) $(TEST_OBJECTS:%.o=%.d)
