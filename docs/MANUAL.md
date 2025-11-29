# WeLearn Downloader Manual

## Author
Shuvam Banerji Seal

## Introduction

Welcome to the WeLearn Downloader, a powerful application designed to help you download and manage your course materials from the WeLearn platform. This tool is available in both a command-line interface (CLI) for advanced users and a graphical user interface (GUI) for those who prefer a more visual approach.

## Features

- **Dual Interface**: Choose between a feature-rich CLI or an intuitive GTK4-based GUI.
- **Automated Downloads**: The application can automatically log in to your WeLearn account, navigate through your courses, and download all your course materials.
- **Selective Downloads**: The CLI provides an interactive mode that allows you to select specific files to download.
- **Organized File Structure**: All downloaded files are automatically organized into folders based on their course names.
- **Credential Storage**: The application can securely store your login credentials for future use.

## Installation

### Arch Linux

This project can be built and installed on Arch Linux using the provided `PKGBUILD` file.

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/shuvam-banerji-seal/welearn-downloader.git
    cd welearn-downloader
    ```

2.  **Build and install the package**:
    ```bash
    makepkg -si
    ```

### Other Linux Distributions

For other Linux distributions, you can build the project from source using the provided `Makefile`.

1.  **Install the dependencies**:
    - `libcurl`
    - `zlib`
    - `openssl`
    - `gtk4` (optional, for the GUI version)

2.  **Build the project**:
    ```bash
    make
    ```

3.  **Install the binaries**:
    ```bash
    sudo make install
    ```

## Usage

### CLI Version

The CLI version of the application can be run from the terminal.

```bash
welearn_cli
```

The CLI will prompt you for your WeLearn username and password. Once you're logged in, you'll be presented with two options:

-   **Download all files**: This option will download all your course materials without any further interaction.
-   **Select specific files to download**: This option will present you with a list of all your course materials and allow you to select which ones to download.

### GUI Version

The GUI version of the application can be run from your desktop environment's application menu or from the terminal.

```bash
welearn_gui
```

The GUI provides a user-friendly interface for logging in, managing your credentials, and downloading your course materials.

## Disclaimer

This program is provided as-is without any warranty. The author is not responsible for any damages, data loss, or issues arising from its use. Use this tool responsibly and ethically, respecting the terms of service of the WeLearn platform.
