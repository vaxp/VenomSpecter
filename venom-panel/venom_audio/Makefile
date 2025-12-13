# Venom Audio Daemon Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include $(shell pkg-config --cflags glib-2.0 gio-2.0 libpulse)
LDFLAGS = $(shell pkg-config --libs glib-2.0 gio-2.0 libpulse)

SRC_DIR = src
BUILD_DIR = build
TARGET = venom_audio

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $(TARGET)..."
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Build complete: $(TARGET)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "📦 Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	@echo "🧹 Cleaning..."
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✅ Clean complete"

install: $(TARGET)
	@echo "📦 Installing..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "✅ Installed to /usr/local/bin/$(TARGET)"
