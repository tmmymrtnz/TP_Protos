TARGET := main

CC := gcc

PACKAGES := check

TARGET_SOURCES := $(shell find src/ -name '*.c')
TEST_SOURCES := $(shell find test/ -name '*.c')

TARGET_OBJECTS := $(TARGET_SOURCES:%.c=%.o)
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -fsanitize=address
LDFLAGS := -lpthread -fsanitize=address $(shell pkg-config --libs $(PACKAGES))
INCLUDES := -Iinclude $(shell pkg-config --cflags $(PACKAGES))

ifdef DEBUG
	CFLAGS += -ggdb3 -O0
else
	ifdef ASAN
		CFLAGS += -ggdb3 -O0 -fsanitize=address
	else
		CFLAGS += -O3
	endif
endif

# Link target
$(TARGET): $(TARGET_OBJECTS)
	$(CC) $(TARGET_OBJECTS) $(CFLAGS) -o $@ $(LDFLAGS)

# Compile target
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

testrunner: $(TEST_OBJECTS) $(TARGET)
	$(CC) $(TEST_OBJECTS) $(CFLAGS) -o $@ $(LDFLAGS)

# Clean up
.PHONY: clean
clean:
	rm -f **/*.o **/*.d
	rm -f $(TARGET) testrunner

-include $(TARGET_OBJECTS:%.o=%.d) $(TEST_OBJECTS:%.o=%.d)
