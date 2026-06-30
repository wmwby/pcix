CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS =
TARGET = pcix
SRC_DIR = src
BUILD_DIR = build

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/bdf.c $(SRC_DIR)/mem.c $(SRC_DIR)/resource.c
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/bdf.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/resource.o

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

.PHONY: all clean test
