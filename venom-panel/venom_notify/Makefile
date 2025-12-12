CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gio-2.0 x11 cairo cairo-xlib pango pangocairo gdk-pixbuf-2.0` -Wall
LIBS = `pkg-config --libs gtk+-3.0 gio-2.0 x11 cairo cairo-xlib pango pangocairo gdk-pixbuf-2.0`

TARGET = venom_notify
SRC = venom_notify.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
