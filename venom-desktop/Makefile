CC      = gcc
CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 gio-2.0) -Iinclude -Wall -Wextra -O2
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 gio-2.0) -lm -ldl

SRCDIR  = src
OBJDIR  = obj
INCDIR  = include

# ---- Main desktop manager (monolithic) ----
TARGET  = venom-desktop-manager
MAIN_SRC = $(SRCDIR)/venom-desktop-manager.c

# ---- setbg tool ----
SETBG_TARGET = venom-setbg
SETBG_SRC    = $(SRCDIR)/venom-setbg.c
SETBG_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null) -Iinclude -Wall -O2
SETBG_LDFLAGS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null) -lImlib2 -lX11

all: $(OBJDIR) $(TARGET) $(SETBG_TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Build desktop manager from the monolithic source
$(TARGET): $(MAIN_SRC)
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

# Build setbg tool
$(SETBG_TARGET): $(SETBG_SRC)
	$(CC) -o $@ $< $(SETBG_CFLAGS) $(SETBG_LDFLAGS)

# ---- Widgets Compilation ----
WIDGETS_DIR_SRC = src/widgets
WIDGET__OUT_DIR = obj/widgets
CONFIG_WIDGET_DIR = $(HOME)/.config/venom/widgets

widgets: $(WIDGET__OUT_DIR)
	@mkdir -p $(CONFIG_WIDGET_DIR)
	@for src in $(WIDGETS_DIR_SRC)/*.c; do \
		if [ -f "$$src" ]; then \
			filename=$$(basename -- "$$src"); \
			name="$${filename%.*}"; \
			echo "Compiling widget: $$name.so"; \
			$(CC) -shared -fPIC -o $(CONFIG_WIDGET_DIR)/$$name.so $$src $(CFLAGS) $(LDFLAGS) ; \
		fi \
	done

$(WIDGET__OUT_DIR):
	mkdir -p $(WIDGET__OUT_DIR)

clean:
	rm -f $(TARGET) $(SETBG_TARGET)
	rm -f $(OBJDIR)/*.o
	rm -f $(CONFIG_WIDGET_DIR)/*.so

.PHONY: all clean widgets
