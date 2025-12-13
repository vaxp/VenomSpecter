CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags gtk+-3.0 x11)
LIBS = $(shell pkg-config --libs gtk+-3.0 x11)

TARGET = vaxp-panel
SRC_DIR = src
BUILD_DIR = build

# Explicit list of sources to match original behavior
SRCS = $(SRC_DIR)/vaxp-panel.c $(SRC_DIR)/launcher.c $(SRC_DIR)/search.c $(SRC_DIR)/pager.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS) -lX11 -lm

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)

.PHONY: all clean
