TARGET := main
DEBUG := y
ASAN := y

CC := gcc
PACKAGES := check

TARGET_SOURCES := $(shell find src/ -name '*.c')
TARGET_OBJECTS := $(TARGET_SOURCES:%.c=%.o)
TARGET_OBJECTS_MINUS_MAIN := $(filter-out src/main.o, $(TARGET_OBJECTS))

TEST_SOURCES := $(shell find test/ -name '*.c')
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

INCLUDES := -Iinclude -Isrc -Isrc/include $(shell pkg-config --cflags $(PACKAGES))
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -MMD $(INCLUDES)
LDFLAGS := -lpthread $(shell pkg-config --libs $(PACKAGES))

ifeq ($(DEBUG), y)
	CFLAGS += -ggdb3 -O0
endif
ifeq ($(ASAN), y)
	CFLAGS += -fsanitize=address
endif
ifneq ($(and $(DEBUG),$(ASAN)),y)
	CFLAGS += -O3
endif

.PHONY: clean test

# Link target
$(TARGET): $(TARGET_OBJECTS)
	$(CC) $(CFLAGS) $(TARGET_OBJECTS) -o $@ $(LDFLAGS)


%_test: test/%_test.o $(TARGET_OBJECTS_MINUS_MAIN)
	$(CC) $(CFLAGS) test/$*_test.o $(TARGET_OBJECTS_MINUS_MAIN) -o $@ $(LDFLAGS)

test: $(TEST_OBJECTS) $(TARGET_OBJECTS_MINUS_MAIN)


clean:
	rm -f **/*.o **/*.d
	rm -f $(TARGET) *_test

-include $(TARGET_OBJECTS:%.o=%.d) $(TEST_OBJECTS:%.o=%.d)
