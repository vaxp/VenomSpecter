CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -Iinclude/logic -Iinclude/ui $(shell pkg-config --cflags gtk+-3.0 x11 xcomposite xrender)
LIBS = $(shell pkg-config --libs gtk+-3.0 x11 xcomposite xrender)

TARGET = vaxp-dock
BUILD_DIR = build

# Source Directories (found recursively)
# Use find to get all directories containing source files for VPATH
SRC_DIRS = $(shell find src -type d)

# Logic Objects
LOGIC_OBJS = $(BUILD_DIR)/search_engine.o \
             $(BUILD_DIR)/pager_service.o \
             $(BUILD_DIR)/window_manager.o \
             $(BUILD_DIR)/app_manager.o

# Helper Objects (shared UI)
UI_HELPER_OBJS = $(BUILD_DIR)/launcher.o \
                 $(BUILD_DIR)/pager.o \
                 $(BUILD_DIR)/search.o

# Main targets
PANEL_OBJS = $(BUILD_DIR)/vaxp-dock.o

all: $(BUILD_DIR) vaxp-dock

vaxp-dock: $(LOGIC_OBJS) $(UI_HELPER_OBJS) $(PANEL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: src/logic/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/ui/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)

.PHONY: all clean
