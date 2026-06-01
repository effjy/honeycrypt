# HoneyCrypt - Decoy based cryptographic system
# Compile targets for Gtk3 desktop Linux development

CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null || echo "")

TARGET = honeycrypt
SRC = honeycrypt.c

all:
	@echo "--- HoneyCrypt C GTK-3 Linux Workspace Builder ---"
	@echo "Run 'make build' to compile the unified application."
	@echo "The compiled binary supports two execution modes:"
	@echo "  - GUI Mode (No arguments):   ./honeycrypt"
	@echo "  - CLI Mode (Arguments):      ./honeycrypt --encrypt \"msg\" <pin> <schema>"
	@echo "                               ./honeycrypt --decrypt <pin> <slots...>"
	@echo "Note: Gtk3 headers (libgtk-3-dev) are required on Linux systems."

build: $(SRC)
	@if [ -z "$(shell which pkg-config)" ]; then \
		echo "pkg-config is missing on this machine. Installing dev build utilities first might be necessary."; \
	fi
	$(CC) -Wall -Wextra -O2 -o $(TARGET) $(SRC) $(shell pkg-config --cflags --libs gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgobject-2.0 -lglib-2.0") -lm

clean:
	rm -f $(TARGET)
