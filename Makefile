C=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11
TARGET=ctimer
SOURCES=ctimer.c
MP3_FILES=start.mp3 end.mp3 finished.mp3
DEST_DIR=$(HOME)/.local/share/ctimer
BIN_DIR=/usr/bin

.PHONY: all clean setup install uninstall

all: $(TARGET) setup

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@

setup:
	mkdir -p $(DEST_DIR)

install: $(TARGET)
	@echo "You may need superuser privileges to install to $(BIN_DIR)"
	sudo cp $(TARGET) $(BIN_DIR)
	sudo cp $(MP3_FILES) $(DEST_DIR)

uninstall:
	@echo "You may need superuser privileges to uninstall from $(BIN_DIR)"
	sudo rm -f $(BIN_DIR)/$(TARGET)
	sudo rm -rf $(DEST_DIR)

clean:
	rm -f $(TARGET)
	rm -rf $(DEST_DIR)
