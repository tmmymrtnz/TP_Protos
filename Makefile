TARGET := main

CC := gcc

PACKAGES := check

TARGET_SOURCES := $(shell find src/ -name '*.c')
TARGET_OBJECTS := $(TARGET_SOURCES:%.c=%.o)
TARGET_OBJECTS_MINUS_MAIN := $(filter-out src/main.o, $(TARGET_OBJECTS))

TEST_SOURCES := $(shell find test/ -name '*.c')
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -fsanitize=address

LDFLAGS := -lpthread -fsanitize=address $(shell pkg-config --libs $(PACKAGES))
INCLUDES := -Iinclude -Isrc -Isrc/include $(shell pkg-config --cflags $(PACKAGES))

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
	$(CC) $(CFLAGS) $(TARGET_OBJECTS) -o $@ $(LDFLAGS)

# Compile target
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@


# buffer_test: test/buffer_test.o $(TARGET_OBJECTS_MINUS_MAIN)
# 	$(CC) $(CFLAGS) test/buffer_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

# hello_test: test/hello_test.o $(TARGET_OBJECTS_MINUS_MAIN)
# 	$(CC) $(CFLAGS) test/hello_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

# netutils_test: test/netutils_test.o $(TARGET_OBJECTS_MINUS_MAIN)
# 	$(CC) $(CFLAGS) test/netutils_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

# parser_test: test/parser_test.o $(TARGET_OBJECTS_MINUS_MAIN)
# 	$(CC) $(CFLAGS) test/parser_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

# parser_utils_test: test/parser_utils_test.o $(TARGET_OBJECTS_MINUS_MAIN)
# 	$(CC) $(CFLAGS) test/parser_utils_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

%_test: test/%_test.o $(TARGET_OBJECTS_MINUS_MAIN)
	$(CC) $(CFLAGS) test/$*_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

test: $(TEST_OBJECTS) $(TARGET_OBJECTS_MINUS_MAIN)

# Clean up
.PHONY: clean
clean:
	rm -f **/*.o **/*.d
	rm -f $(TARGET) testrunner

-include $(TARGET_OBJECTS:%.o=%.d) $(TEST_OBJECTS:%.o=%.d)
