# Quick Start Guide - WeLearn Downloader

## Installation

### Ubuntu/Debian (Quick)
```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev zlib1g-dev libssl-dev libgtk-4-dev

# 2. Build
make

# 3. Run GUI version
./welearn_gui

# Or run CLI version
./welearn_cli
```

### Fedora/RHEL/CentOS
```bash
# 1. Install dependencies
sudo dnf install -y curl-devel zlib-devel openssl-devel gtk4-devel

# 2. Build
make

# 3. Run
./welearn_gui  # or ./welearn_cli
```

## Usage

### GUI Version (Recommended)

1. Launch the application:
   ```bash
   ./welearn_gui
   ```

2. Enter your WeLearn credentials:
   - Username: Your WeLearn username
   - Password: Your WeLearn password

3. (Optional) Check "Save credentials" to remember your login

4. Click "Download Courses"

5. Monitor progress in the log window

6. Files will be downloaded to folders in the current directory

### CLI Version

1. Run the application:
   ```bash
   ./welearn_cli
   ```

2. Enter credentials when prompted (or they'll be loaded if saved)

3. The application will automatically:
   - Log in to WeLearn
   - Discover all courses
   - Download all course materials
   - Organize files by course

## Directory Structure After Download

```
your-directory/
├── Course_Name_1/
│   ├── lecture1.pdf
│   ├── assignment1.pdf
│   └── ...
├── Course_Name_2/
│   ├── notes.pdf
│   └── ...
└── ...
```

## Build Options

### Build only CLI (no GTK4 required)
```bash
make cli
```

### Build only GUI (requires GTK4)
```bash
make gui
```

### Clean build
```bash
make clean
make
```

### Install system-wide
```bash
sudo make install
```

## Troubleshooting

### Issue: GTK4 not found
**Solution:** Install GTK4 development libraries:
```bash
sudo apt-get install libgtk-4-dev  # Ubuntu/Debian
sudo dnf install gtk4-devel         # Fedora/RHEL
```

### Issue: libcurl not found
**Solution:** Install libcurl development files:
```bash
sudo apt-get install libcurl4-openssl-dev  # Ubuntu/Debian
sudo dnf install curl-devel                # Fedora/RHEL
```

### Issue: Login fails
- Verify credentials are correct
- Check internet connection
- Ensure WeLearn website is accessible
- Try deleting `credentials.dat` and re-entering credentials

### Issue: Permission denied on directory creation
**Solution:** Run from a directory where you have write permissions:
```bash
cd ~/Downloads
./welearn_gui
```

## Features

- ✅ Modern GTK4 graphical interface
- ✅ Command-line interface
- ✅ Automatic course discovery
- ✅ Smart file naming
- ✅ Duplicate detection
- ✅ Folder recursion
- ✅ Progress tracking
- ✅ Activity logging
- ✅ Credential storage (basic encryption)

## Security Warning

⚠️ **Important:** This tool uses simple XOR encryption for credential storage, which is **NOT SECURE**. Do not use with sensitive accounts or on shared computers.

## Support

For issues or questions:
1. Check the main README.md
2. Review error messages in the log window
3. Open an issue on GitHub

## Quick Tips

- **First time use**: Run the GUI version for easier setup
- **Automation**: Use CLI version in scripts
- **Large downloads**: Be patient, it may take time
- **Rate limiting**: The tool includes delays to avoid being blocked
- **Updates**: Pull latest changes and rebuild with `make clean all`
