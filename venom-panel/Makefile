CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags gtk+-3.0 gio-2.0 libnm)
LIBS = $(shell pkg-config --libs gtk+-3.0 gio-2.0 libnm) -lX11 -lpulse

SRCDIR = src
OBJDIR = obj
INCDIR = include

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
TARGET = venom-panel

all: $(OBJDIR) $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -rf $(OBJDIR)

.PHONY: all clean
