
# WeLearn Downloader

This C program automates the downloading of resources from the WeLearn platform. It handles login, navigates through course pages, and downloads files.

**Warning:** This tool uses a simple XOR encryption for storing credentials, which is **NOT SECURE**. Do not use it with sensitive accounts.

## Table of Contents

* [Features](#features)
* [Dependencies](#dependencies)
* [Installation](#installation)
* [Configuration](#configuration)
* [Usage](#usage)
* [Compilation](#compilation)
* [Limitations](#limitations)
* [Disclaimer](#disclaimer)

## Features

* Logs into the WeLearn platform.
* Navigates course pages.
* Extracts file download links.
* Downloads files, handling filename extraction from headers and URLs.
* Saves and loads credentials (using weak encryption).
* Creates directories for courses.
* Tracks visited URLs to avoid redundant processing.

## Dependencies

* **libcurl:** A library for making HTTP requests.  You'll need both the library and the header files.
* **zlib:** A library for data compression.
* **OpenSSL:** A cryptography library.

## Installation

1.  **Install libcurl:**
    * **Ubuntu/Debian:** `sudo apt-get install libcurl4-openssl-dev`
    * **Fedora/CentOS/RHEL:** `sudo dnf install curl-devel`
    * **Windows:** Download a pre-built binary from the official libcurl website and set up the include paths and library linking in your compiler.
2.  **Install zlib:**
    * **Ubuntu/Debian:** `sudo apt-get install zlib1g-dev`
    * **Fedora/CentOS/RHEL:** `sudo dnf install zlib-devel`
    * **Windows:** You might need to download and compile zlib, or find a pre-built binary.
3.  **Install OpenSSL:**
    * **Ubuntu/Debian:** `sudo apt-get install libssl-dev`
    * **Fedora/CentOS/RHEL:** `sudo dnf install openssl-devel`
    * **Windows:** Download and install OpenSSL. You'll need to configure your compiler's include and lib paths.

## Configuration

* The program saves credentials in a file named `credentials.dat`.
* The encryption key used for this file is defined as `ENCRYPTION_KEY 'S'`.  **DO NOT CHANGE THIS (or at least change it back before committing), AND BE AWARE THIS IS NOT SECURE!**

## Usage

1.  **Compile** the code (see [Compilation](#compilation)).
2.  **Run** the executable.
3.  The program will prompt you for your WeLearn username and password.
4.  It will then attempt to log in and download resources.
5.  Downloaded files will be saved in directories corresponding to the course names.


## Compilation

```bash
gcc -o welearn_downloader welearn_downloader.c -lcurl
```

## Windows:

You'll likely need to add compiler flags to specify the location of the libcurl, zlib, and OpenSSL header files and libraries.  This usually involves the `-I` flag for include directories and the `-L` flag for library directories, along with `-l` to link to specific libraries (e.g., `-lcurl`, `-lzlib`, `-lssl`, `-lcrypto`).  Consult your compiler's documentation (e.g., MinGW, Visual Studio) for details.

## Limitations

* **Security:** The credential saving is extremely insecure.  Do not use this with sensitive accounts.
* **Error Handling:** While there is some error handling, it might not be robust enough for all situations.
* **Website Changes:** If the WeLearn website's structure changes, the program may break.
* **No GUI:** This is a command-line tool.
* **Rate Limiting:** The script does not implement any delay and can be detected as a bot and get your IP banned. Add some delay.

## Disclaimer

This program is provided as-is. Use it at your own risk. The author is not responsible for any damages or issues arising from its use.  Use this tool responsibly and ethically, respecting the terms of service of the WeLearn platform.
