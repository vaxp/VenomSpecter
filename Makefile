CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 x11)
LIBS = $(shell pkg-config --libs gtk+-3.0 x11)

TARGET = vaxp-panel
SRC = vaxp-panel.c launcher.c search.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
