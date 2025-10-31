# Makefile for WeLearn Downloader
# Supports both CLI and GUI (GTK4) versions

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lcurl -lpthread

# Source files
COMMON_SRC = src/welearn_common.c src/welearn_auth.c src/welearn_download.c
CLI_SRC = src/welearn_cli.c
GUI_SRC = src/welearn_gui.c

# Object files
COMMON_OBJ = $(COMMON_SRC:.c=.o)
CLI_OBJ = $(CLI_SRC:.c=.o)
GUI_OBJ = $(GUI_SRC:.c=.o)

# Executables
CLI_TARGET = welearn_cli
GUI_TARGET = welearn_gui

# GTK4 flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

# Default target
all: $(CLI_TARGET) gui-check

# CLI version (always built)
$(CLI_TARGET): $(COMMON_OBJ) $(CLI_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "CLI version built successfully: $(CLI_TARGET)"

# GUI version (only if GTK4 is available)
gui-check:
	@if pkg-config --exists gtk4; then \
		echo "GTK4 found, building GUI version..."; \
		$(MAKE) $(GUI_TARGET); \
	else \
		echo "GTK4 not found. Install with: sudo apt-get install libgtk-4-dev"; \
		echo "Only CLI version will be available."; \
	fi

$(GUI_TARGET): $(COMMON_OBJ) $(GUI_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(GTK_LIBS)
	@echo "GUI version built successfully: $(GUI_TARGET)"

# Compile common source files
src/welearn_common.o: src/welearn_common.c include/welearn_common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/welearn_auth.o: src/welearn_auth.c include/welearn_auth.h include/welearn_common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/welearn_download.o: src/welearn_download.c include/welearn_download.h include/welearn_common.h include/welearn_auth.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile CLI source
src/welearn_cli.o: src/welearn_cli.c include/welearn_common.h include/welearn_auth.h include/welearn_download.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile GUI source
src/welearn_gui.o: src/welearn_gui.c include/welearn_common.h include/welearn_auth.h include/welearn_download.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f src/*.o $(CLI_TARGET) $(GUI_TARGET)
	rm -f cookies.txt credentials.dat
	@echo "Clean complete"

# Clean only object files
clean-obj:
	rm -f src/*.o
	@echo "Object files cleaned"

# Install targets (optional)
install: all
	@echo "Installing to /usr/local/bin..."
	sudo cp $(CLI_TARGET) /usr/local/bin/
	@if [ -f $(GUI_TARGET) ]; then \
		sudo cp $(GUI_TARGET) /usr/local/bin/; \
	fi
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling from /usr/local/bin..."
	sudo rm -f /usr/local/bin/$(CLI_TARGET)
	sudo rm -f /usr/local/bin/$(GUI_TARGET)
	@echo "Uninstallation complete"

# Help target
help:
	@echo "WeLearn Downloader Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build CLI version and GUI (if GTK4 available)"
	@echo "  cli        - Build only CLI version"
	@echo "  gui        - Build only GUI version (requires GTK4)"
	@echo "  clean      - Remove all build artifacts"
	@echo "  clean-obj  - Remove only object files"
	@echo "  install    - Install binaries to /usr/local/bin"
	@echo "  uninstall  - Remove installed binaries"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build all versions"
	@echo "  make cli          # Build only CLI"
	@echo "  make clean all    # Clean and rebuild"

# Explicit CLI-only target
cli: $(CLI_TARGET)

# Explicit GUI-only target (will fail if GTK4 not available)
gui: $(GUI_TARGET)

.PHONY: all clean clean-obj install uninstall help cli gui gui-check
