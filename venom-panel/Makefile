CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 gio-2.0 libnm)
LIBS = $(shell pkg-config --libs gtk+-3.0 gio-2.0 libnm) -lX11 -lpulse

SRC = venom-panel.c clock-widget.c system-icons.c control-center.c wifi-manager.c bluetooth-manager.c notification-service.c brightness-manager.c search.c
OBJ = $(SRC:.c=.o)
TARGET = venom-panel

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
