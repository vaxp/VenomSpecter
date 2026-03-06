CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags gtk+-3.0 gio-2.0 libnm)
LIBS = $(shell pkg-config --libs gtk+-3.0 gio-2.0 libnm) -lX11 -lpulse -lm -ldl

TARGET_PANEL = venom-panel
TARGET_SETTINGS = venom-panel-settings
SRCDIR = src
OBJDIR = obj
INCDIR = include

SRC = $(wildcard $(SRCDIR)/*.c)
# Filter out venom-panel-settings.c because it's built independently
PANEL_SRC = $(filter-out $(SRCDIR)/venom-panel-settings.c, $(SRC))
PANEL_OBJ = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(PANEL_SRC))

all: $(OBJDIR) $(TARGET_PANEL) $(TARGET_SETTINGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET_PANEL): $(PANEL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TARGET_SETTINGS): $(SRCDIR)/venom-panel-settings.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET_PANEL) $(TARGET_SETTINGS)
	rm -rf $(OBJDIR)

panel-plugins:
	@mkdir -p $(HOME)/.config/venom/panel-plugins
	@for f in src/panel-plugins/*.c; do \
	    name=$$(basename $$f .c); \
	    echo "Compiling panel plugin: $$name.so"; \
	    $(CC) -shared -fPIC -o $(HOME)/.config/venom/panel-plugins/$$name.so $$f \
	        $(CFLAGS) $(shell pkg-config --libs gtk+-3.0 gio-2.0); \
	done

.PHONY: all clean panel-plugins
