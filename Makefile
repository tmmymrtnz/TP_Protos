TARGET := main

CC := gcc

TARGET_SOURCES := $(shell find src/ -name '*.c')
TEST_SOURCES := $(shell find test/ -name '*.c')

TARGET_OBJECTS := $(TARGET_SOURCES:%.c=%.o)
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)

CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -lpthread
ifdef DEBUG
	CFLAGS += -ggdb3 -O0
else
	ifdef ASAN
		CFLAGS += -ggdb3 -O0 -fsanitize=address
	else
		CFLAGS += -O3
	endif
endif

# INCLUDES := 
# LDFLAGS := 


# Link target
$(TARGET): $(TARGET_OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) $(TARGET_OBJECTS) -o $@ $(LDFLAGS)

# Compile target
$(TARGET_OBJECTS): $(TARGET_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -o $@ -c $<


testrunner: $(TEST_OBJECTS) $(TARGET)
	$(CC) $(CFLAGS) $(TEST_OBJECTS) -o $@

$(TEST_OBJECTS): $(TEST_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -o $@ -c $<




.PHONY: clean
clean:
	rm -f **/*.o **/*.d
	rm -f $(TARGET) testrunner

-include $(TARGET_OBJECTS:%.o=%.d) $(TEST_OBJECTS:%.o=%.d)
