CC = cc
CFLAGS = -Wall -Wextra
TARGET = ctimer
SOURCES = ctimer.c
MP3_FILES = start.mp3 end.mp3 over.mp3
MAN_PAGE = ctimer.1

DEST_DIR = $(HOME)/.local/share/ctimer
BIN_DIR = /usr/local/bin
MAN_DIR = /usr/local/share/man/man1

# If ENABLE_HERBE is set to 1, append -herbe to CFLAGS:
ifeq ($(ENABLE_HERBE),1)
CFLAGS += -DHERBE
endif

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $<

install: $(TARGET)
	mkdir -p $(DEST_DIR) $(MAN_DIR)
	cp $(TARGET) $(BIN_DIR)/
	cp $(MP3_FILES) $(DEST_DIR)/
	cp $(MAN_PAGE) $(MAN_DIR)/
	mandb

uninstall:
	rm -f $(BIN_DIR)/$(TARGET)
	rm -rf $(DEST_DIR)

clean:
	rm -f $(TARGET)
