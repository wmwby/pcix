CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS =
TARGET = pcix
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
TEST_BUILD_DIR = $(BUILD_DIR)/tests

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/bdf.c $(SRC_DIR)/mem.c $(SRC_DIR)/resource.c $(SRC_DIR)/memmap.c $(SRC_DIR)/dump.c
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/bdf.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/resource.o $(BUILD_DIR)/memmap.o $(BUILD_DIR)/dump.o

# Test files
TEST_SOURCES = $(TEST_DIR)/test_memmap.c $(TEST_DIR)/test_dump.c
TEST_TARGETS = $(BUILD_DIR)/test_memmap $(BUILD_DIR)/test_dump

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(BUILD_DIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

test: $(TARGET)
	./$(TARGET) -h

test-unit: $(TEST_TARGETS)
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

$(TEST_BUILD_DIR):
	@mkdir -p $(TEST_BUILD_DIR)

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(TEST_BUILD_DIR) $(OBJECTS)
	$(CC) $(CFLAGS) $< -o $@ $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))

.PHONY: all clean test test-unit
