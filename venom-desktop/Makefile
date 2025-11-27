# Makefile for Venom Desktop Manager

CC = gcc
CFLAGS = $(shell pkg-config --cflags gtk+-3.0 gio-2.0) -Wall
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 gio-2.0) -lm

TARGET = venom-desktop-manager
SOURCES = main.c wallpaper.c icons.c filesystem.c selection.c menu.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = wallpaper.h icons.h filesystem.h selection.h menu.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
