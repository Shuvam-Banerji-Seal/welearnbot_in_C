# WeLearn Downloader

A modern C application for automating resource downloads from the WeLearn platform. Available in both command-line (CLI) and graphical (GTK4 GUI) versions.

**Warning:** This tool uses simple XOR encryption for storing credentials, which is **NOT SECURE**. Do not use it with sensitive accounts.

## Features

* **GUI Version** - Modern GTK4 graphical interface with:
  - User-friendly login form
  - Real-time download progress
  - Activity log viewer
  - Credential management
* **CLI Version** - Enhanced command-line interface with:
  - **Interactive file selection** - Choose specific files to download
  - **Tree/List view** - View files hierarchically or in simple list format
  - **Custom download directory** - Specify where files should be saved
  - **Selective downloading** - Download all or specific files by number
  - Traditional "download all" mode for quick batch downloads
* Automated login and session management
* Course navigation and resource extraction
* Smart file naming and organization
* Duplicate detection
* Folder recursion support
* Credential storage (basic encryption)

## Project Structure

```
welearnbot_in_C/
├── include/              # Header files
│   ├── welearn_common.h  # Common structures and utilities
│   ├── welearn_auth.h    # Authentication functions
│   └── welearn_download.h # Download logic
├── src/                  # Source files
│   ├── welearn_common.c  # Common utilities implementation
│   ├── welearn_auth.c    # Authentication implementation
│   ├── welearn_download.c # Download implementation
│   ├── welearn_cli.c     # CLI application
│   └── welearn_gui.c     # GTK4 GUI application
├── build/                # Build artifacts (created during build)
├── docs/                 # Documentation
├── Makefile             # Build system
└── README.md            # This file
```

## Dependencies

### Required (for CLI version)
* **libcurl** - HTTP client library
  - Ubuntu/Debian: `sudo apt-get install libcurl4-openssl-dev`
  - Fedora/CentOS/RHEL: `sudo dnf install curl-devel`
* **zlib** - Compression library
  - Ubuntu/Debian: `sudo apt-get install zlib1g-dev`
  - Fedora/CentOS/RHEL: `sudo dnf install zlib-devel`
* **OpenSSL** - Cryptography library
  - Ubuntu/Debian: `sudo apt-get install libssl-dev`
  - Fedora/CentOS/RHEL: `sudo dnf install openssl-devel`

### Optional (for GUI version)
* **GTK4** - GUI toolkit (version 4.x)
  - Ubuntu/Debian: `sudo apt-get install libgtk-4-dev`
  - Fedora/CentOS/RHEL: `sudo dnf install gtk4-devel`

## Installation

### Quick Install (Ubuntu/Debian)

```bash
# Install all dependencies (including GUI)
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev zlib1g-dev libssl-dev libgtk-4-dev

# Build both versions
make

# Or install system-wide
make install
```

### Build Options

```bash
# Build both CLI and GUI (if GTK4 available)
make

# Build only CLI version
make cli

# Build only GUI version
make gui

# Clean and rebuild
make clean all

# Show all available targets
make help
```

## Usage

### GUI Version

Simply run the graphical application:

```bash
./welearn_gui
```

The GUI provides:
1. **Login Form** - Enter your WeLearn credentials
2. **Save Credentials** - Option to save credentials for future use
3. **Download Button** - Start downloading all courses
4. **Progress Bar** - Visual feedback on download progress
5. **Activity Log** - Real-time log of all operations

### CLI Version

Run the command-line application:

```bash
./welearn_cli
```

#### Interactive Mode (NEW!)

The CLI now offers two modes:

**Mode 1: Download All Files (Original Behavior)**
- Automatically downloads all files from all courses
- Downloads to current directory
- No user interaction required

**Mode 2: Interactive File Selection (NEW)**

When you select Mode 2, you get:

1. **Course Scanning** - Scans all enrolled courses and collects file information
2. **Display Options**:
   - **Tree View**: Hierarchical display with folders and files organized by course
   - **List View**: Simple table showing all files with course names
3. **Custom Download Directory** - Choose where to save files (default: current directory)
4. **File Selection**:
   - Enter `all` to download all files
   - Enter specific file numbers separated by commas (e.g., `1,3,5,7`)
   - Enter `q` to quit without downloading

#### Example Usage Flow

```bash
$ ./welearn_cli

# After login...
Choose an option:
1. Download all files (old behavior)
2. Select specific files to download (new)

Enter choice (1 or 2): 2

# Files are scanned...
How would you like to view the files?
1. Tree view (hierarchical)
2. List view (simple table)

Enter choice (1 or 2): 1

# Tree view displayed...
📚 Course: Data Structures
  📄 [1] Lecture_1_Introduction.pdf
  📄 [2] Assignment_1.pdf
  📁 [3] Week_2_Materials (folder)
    📄 [4] Lecture_2.pdf

Enter download directory path (press Enter for current directory '.'): ./downloads

Select files to download:
  - Enter 'all' to download all files
  - Enter file numbers separated by commas (e.g., 1,3,5,7)
  - Enter 'q' to quit without downloading

Your selection: 1,2,4

# Downloads only files 1, 2, and 4...
```

#### Legacy Behavior

The original "download everything" behavior is preserved as Mode 1 for users who prefer the automatic approach.

## Configuration

### Credential Storage

* Credentials are saved to `credentials.dat` (encrypted with XOR)
* The encryption key is defined as `ENCRYPTION_KEY 'S'`
* **Important**: This is NOT secure encryption - use at your own risk

### Download Location

* Files are downloaded to the current directory
* Each course gets its own folder (named after the course title)
* Existing files are automatically skipped

## Building from Source

### Prerequisites

Ensure you have:
* GCC compiler (or compatible C compiler)
* Make build system
* Development libraries installed (see Dependencies)

### Compilation

The Makefile automatically detects available libraries:

```bash
# Standard build
make

# Verbose build (shows compilation commands)
make VERBOSE=1

# Build with debug symbols
make CFLAGS="-Wall -Wextra -g -Iinclude"
```

### Cross-Platform Notes

#### Arch Linux
An Arch Linux package is available in the root of this repository. You can build and install it using the following commands:
```bash
# Build and install the package
makepkg -si
```

#### Linux
The default target platform. All features fully supported.

#### Windows
You'll need to:
1. Install MinGW or similar compiler
2. Install libcurl, zlib, OpenSSL for Windows
3. Adjust include and library paths in the Makefile
4. Use `mingw32-make` instead of `make`

#### macOS
Install dependencies via Homebrew:
```bash
brew install curl openssl gtk4
```

## Architecture

The application is modularized into several components:

### Core Modules

1. **welearn_common** - Shared utilities
   - Memory management for HTTP responses
   - URL visited tracking
   - File/path sanitization
   - Directory creation

2. **welearn_auth** - Authentication
   - Login token extraction
   - Credential encryption/decryption
   - Secure password input
   - Credential persistence

3. **welearn_download** - Download logic
   - File downloading with resume support
   - Course page parsing
   - Folder recursion
   - Smart filename detection

4. **welearn_cli** - CLI interface
   - Command-line user interaction
   - Terminal output formatting

5. **welearn_gui** - GTK4 GUI
   - Modern graphical interface
   - Multi-threaded downloads
   - Progress indication
   - Log viewer

## Limitations

* **Security**: Credential storage uses weak XOR encryption
* **Website Changes**: May break if WeLearn's HTML structure changes
* **Rate Limiting**: No delay between requests (could trigger anti-bot measures)
* **Error Recovery**: Limited handling of network interruptions
* **Platform Support**: GUI requires GTK4 (Linux primarily)

## Troubleshooting

### Build Issues

**Problem**: `Package gtk4 was not found`
```bash
# Solution: Install GTK4 development libraries
sudo apt-get install libgtk-4-dev
```

**Problem**: `curl/curl.h: No such file or directory`
```bash
# Solution: Install libcurl development files
sudo apt-get install libcurl4-openssl-dev
```

### Runtime Issues

**Problem**: "Failed to initialize libcurl"
```bash
# Ensure libcurl is installed
sudo apt-get install libcurl4
```

**Problem**: GUI doesn't start
```bash
# Check GTK4 runtime is installed
sudo apt-get install libgtk-4-1

# Run with debug info
GTK_DEBUG=interactive ./welearn_gui
```

**Problem**: Login fails
- Verify your credentials are correct
- Check if WeLearn website is accessible
- Ensure cookies.txt can be created in the current directory

## Development

### Code Style
- Follow Linux kernel coding style
- Use 4 spaces for indentation
- Comment complex logic
- Keep functions focused and modular

### Adding Features
1. Add new headers to `include/` if needed
2. Implement in appropriate `src/` file
3. Update Makefile if adding new files
4. Update README with new features

### Testing
```bash
# Build with debug symbols
make clean
make CFLAGS="-Wall -Wextra -g -Iinclude"

# Run with valgrind (memory leak detection)
valgrind --leak-check=full ./welearn_cli

# Run with gdb (debugging)
gdb ./welearn_cli
```

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is provided as-is for educational purposes. Use responsibly and ethically.

## Disclaimer

This program is provided as-is without any warranty. The author is not responsible for any damages, data loss, or issues arising from its use. Use this tool responsibly and ethically, respecting the terms of service of the WeLearn platform.

## Acknowledgments
* **Author**: Shuvam Banerji Seal
* Built with GTK4 for the GUI
* Uses libcurl for HTTP operations
* Inspired by the need for efficient course material management
