CC      = gcc
CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 gio-2.0) -Iinclude -Wall -Wextra -O2
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 gio-2.0) -lm

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

clean:
	rm -f $(TARGET) $(SETBG_TARGET)
	rm -f $(OBJDIR)/*.o

.PHONY: all clean
