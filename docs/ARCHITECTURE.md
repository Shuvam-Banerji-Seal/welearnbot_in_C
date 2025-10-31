# Architecture Documentation

## System Architecture

The WeLearn Downloader has been refactored into a modular, maintainable architecture with clear separation of concerns.

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
├──────────────────────────┬──────────────────────────────────┤
│    welearn_gui.c         │       welearn_cli.c              │
│  (GTK4 GUI Interface)    │   (Command-line Interface)       │
└──────────┬───────────────┴────────────┬─────────────────────┘
           │                            │
           └────────────┬───────────────┘
                        │
           ┌────────────▼───────────────┐
           │   Business Logic Layer     │
           ├────────────────────────────┤
           │  welearn_download.c/h      │
           │  - Course extraction       │
           │  - Resource downloading    │
           │  - Page processing         │
           │  - Folder recursion        │
           └────────────┬───────────────┘
                        │
           ┌────────────▼───────────────┐
           │  welearn_auth.c/h          │
           │  - Login token extraction  │
           │  - Credential management   │
           │  - Password encryption     │
           └────────────┬───────────────┘
                        │
           ┌────────────▼───────────────┐
           │  welearn_common.c/h        │
           │  - HTTP memory management  │
           │  - File/path utilities     │
           │  - URL tracking            │
           │  - Directory operations    │
           └────────────┬───────────────┘
                        │
           ┌────────────▼───────────────┐
           │   External Libraries       │
           ├────────────────────────────┤
           │  - libcurl (HTTP)          │
           │  - GTK4 (GUI)              │
           │  - pthread (Threading)     │
           │  - OpenSSL (Crypto)        │
           └────────────────────────────┘
```

## Module Descriptions

### 1. welearn_common (Foundation Layer)
**Purpose:** Provides shared utilities used across all components.

**Responsibilities:**
- Memory management for HTTP responses
- URL visited tracking (prevent infinite loops)
- Filename sanitization
- Directory creation and validation
- Cross-platform compatibility layer

**Key Functions:**
- `init_memory_struct()` - Initialize memory buffer
- `write_memory_callback()` - libcurl callback for data
- `sanitize_filename()` - Clean filenames for filesystem
- `create_directory()` - Safe directory creation
- `init_visited_urls()` - URL tracking initialization

### 2. welearn_auth (Authentication Layer)
**Purpose:** Handles all authentication and credential management.

**Responsibilities:**
- Extract login tokens from HTML
- Encrypt/decrypt credentials (XOR)
- Secure password input (no echo)
- Credential persistence

**Key Functions:**
- `extract_logintoken()` - Parse login token from HTML
- `get_password()` - Secure password input
- `save_credentials()` - Encrypt and save to file
- `load_credentials()` - Load and decrypt from file
- `encrypt_decrypt()` - Simple XOR encryption

**Security Note:** Uses basic XOR encryption (NOT SECURE for production).

### 3. welearn_download (Business Logic Layer)
**Purpose:** Core download and processing logic.

**Responsibilities:**
- Course link extraction
- Page parsing for resources
- File downloading with metadata
- Folder recursion
- Progress tracking

**Key Functions:**
- `download_file()` - Download individual file
- `process_page_for_resources()` - Parse page for links
- `extract_course_title()` - Get course name from HTML
- `extract_course_links_and_process()` - Main processing loop

**Features:**
- Smart filename detection (headers, URL, suggested)
- Duplicate file detection
- Folder navigation
- Progress reporting

### 4. welearn_cli (CLI Interface)
**Purpose:** Traditional command-line user interface.

**Features:**
- Interactive credential input
- Progress output to console
- Reuses all core components
- Scriptable/automatable

**Usage Flow:**
1. Initialize libcurl
2. Load or prompt for credentials
3. Fetch login page and extract token
4. Perform login POST request
5. Call `extract_course_links_and_process()`
6. Clean up and exit

### 5. welearn_gui (GUI Interface)
**Purpose:** Modern GTK4 graphical user interface.

**Features:**
- Login form with password masking
- Save credentials checkbox
- Real-time progress bar
- Scrollable activity log
- Multi-threaded downloads (non-blocking)

**Architecture:**
- Main thread: GTK event loop
- Worker thread: Download operations
- `g_idle_add()` for UI updates from worker thread

**Components:**
- `AppState` - Application state container
- `DownloadThreadData` - Thread communication
- `activate()` - Application setup
- `on_login_clicked()` - Login handler
- `download_thread_func()` - Background worker

## Data Flow

### Login Flow
```
User Input → welearn_auth.load_credentials()
           → welearn_auth.extract_logintoken(html)
           → libcurl POST with credentials
           → Session cookies saved
```

### Download Flow
```
Dashboard HTML → welearn_download.extract_course_links_and_process()
              → For each course:
                 → welearn_download.extract_course_title()
                 → welearn_common.create_directory()
                 → welearn_download.process_page_for_resources()
                    → For each resource:
                       → welearn_download.download_file()
                          → welearn_common.sanitize_filename()
                          → libcurl GET request
                          → Write to file
```

### URL Tracking (Prevent Loops)
```
welearn_common.init_visited_urls()
  → Before processing URL:
     → welearn_common.is_url_visited()
        → If visited: Skip
        → If new: welearn_common.add_visited_url()
                → Process URL
```

## Build System

### Makefile Structure
```
make
  ├── Detect GTK4
  ├── Build common objects
  ├── Build CLI (always)
  └── Build GUI (if GTK4 available)
```

### Dependency Tree
```
welearn_cli → welearn_common.o
            → welearn_auth.o
            → welearn_download.o
            → libcurl

welearn_gui → welearn_common.o
            → welearn_auth.o
            → welearn_download.o
            → libcurl
            → GTK4 libs
            → pthread
```

## Threading Model (GUI)

### Main Thread
- GTK event loop
- UI rendering
- User input handling

### Worker Thread
- HTTP requests (libcurl)
- File downloads
- Course processing
- Updates main thread via `g_idle_add()`

### Thread Safety
- No shared mutable state between threads
- UI updates only from main thread
- CURL handle per thread
- Thread data passed via malloc'd struct

## Error Handling

### Strategy
1. **Fail Fast:** Return NULL/0 on errors
2. **Cleanup:** Always free allocated resources
3. **Logging:** Print debug messages to stderr
4. **Graceful Degradation:** Continue on non-critical errors

### Example Patterns
```c
// Memory allocation
ptr = malloc(size);
if (!ptr) {
    perror("malloc failed");
    return NULL;
}

// File operations
fp = fopen(path, "wb");
if (!fp) {
    perror("fopen failed");
    free(ptr);
    return 0;
}

// Network requests
res = curl_easy_perform(curl);
if (res != CURLE_OK) {
    fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
    cleanup();
    return;
}
```

## Platform Support

### Linux (Primary)
- Full support for all features
- Native GTK4 support
- POSIX APIs

### Windows
- Conditional compilation with `#ifdef _WIN32`
- Alternative implementations for:
  - Directory creation (`_mkdir`)
  - Password input (`_getch`)
  - Sleep function
- GTK4 support via MSYS2/MinGW

### macOS
- Full support via Homebrew dependencies
- Native POSIX APIs
- GTK4 via Homebrew

## Future Enhancements

### Potential Improvements
1. **Security:** Replace XOR with proper encryption (AES)
2. **Resume Support:** Save download state for interrupted downloads
3. **Rate Limiting:** Configurable delays to avoid bans
4. **Filters:** Allow selective course/file downloads
5. **Notifications:** Desktop notifications on completion
6. **Progress Detail:** Per-file progress bars
7. **Settings:** Configuration file for customization
8. **Logging:** Structured logging to file
9. **Tests:** Unit tests for core components
10. **Localization:** Multi-language support

### Architecture Extensibility
The modular design makes it easy to:
- Add new download sources
- Implement different authentication methods
- Create alternative UIs (web interface?)
- Add plugins or extensions
- Integrate with other tools

## Performance Considerations

### Memory Management
- Realloc strategy for growing buffers
- Proper cleanup in error paths
- No memory leaks (validated with valgrind)

### Network Efficiency
- Reuse CURL handle
- Cookie persistence
- Connection keep-alive
- Conditional requests (future)

### GUI Responsiveness
- Non-blocking downloads
- Incremental UI updates
- Efficient text buffer operations

## Code Style

### Conventions
- **Naming:** `snake_case` for functions and variables
- **Indentation:** 4 spaces
- **Braces:** K&R style
- **Comments:** Explain why, not what
- **Headers:** Include guards, forward declarations

### File Organization
```
file.h:
  - Include guards
  - Includes
  - Constants
  - Structures
  - Function declarations

file.c:
  - Includes (system, then local)
  - Static helpers (forward declared)
  - Public functions
  - Cleanup/utility functions
```

## Conclusion

This architecture provides:
✅ **Modularity** - Clear separation of concerns
✅ **Maintainability** - Easy to understand and modify
✅ **Extensibility** - Simple to add features
✅ **Testability** - Components can be tested independently
✅ **Reusability** - Common code shared between CLI and GUI
✅ **Professional Structure** - Industry-standard organization
