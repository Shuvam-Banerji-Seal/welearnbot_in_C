#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>     // For getch (Unix), sleep
#include <sys/stat.h>   // For mkdir
#include <errno.h>      // For errno
#include <ctype.h>      // For isalnum
#include <time.h>       // For fallback filenames

#ifdef _WIN32
#include <windows.h>
#include <conio.h>      // For _getch()
#define MKDIR(path) _mkdir(path)
#define SLEEP(seconds) Sleep(seconds * 1000)
#else
// For Unix-like systems
#include <termios.h>    // For terminal control functions
#define MKDIR(path) mkdir(path, 0777) // Read, write, execute for everyone
#define SLEEP(seconds) sleep(seconds)
#endif

#define MAX_PATH_LEN 1024
#define MAX_URL_LEN 2048
#define MAX_FILENAME_LEN 256
#define CRED_FILE "credentials.dat"
#define ENCRYPTION_KEY 'S' // Simple XOR key - NOT SECURE!
#define INITIAL_VISITED_CAPACITY 50 // Initial capacity for visited URLs array

// Structure for storing fetched data in memory
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Structure for storing filename extracted from headers
struct HeaderData {
    char filename[MAX_FILENAME_LEN];
};

// --- Structure for Visited URL Tracking ---
struct VisitedUrls {
    char **urls;    // Array of URL strings
    size_t count;   // Number of URLs currently stored
    size_t capacity;// Current allocated capacity of the array
};


// --- Forward Declarations ---
void init_memory_struct(struct MemoryStruct *chunk);
size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);
size_t write_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
size_t write_data_callback(void *ptr, size_t size, size_t nmemb, FILE *stream);
char *extract_logintoken(const char *html);
void encrypt_decrypt(char *text, char key);
void get_password(char *password, size_t size);
int save_credentials(const char *username, const char *password, char key);
int load_credentials(char *username, size_t user_size, char *password, size_t pass_size, char key);
int create_directory(const char *path);
char* sanitize_filename(const char* input_filename, char* output_filename, size_t output_size);
void extract_filename_from_url(const char *url, char *filename, size_t size);
void download_file(CURL *curl, const char *url, const char *course_path, const char* suggested_name);
// Visited URL functions
void init_visited_urls(struct VisitedUrls *visited);
int add_visited_url(struct VisitedUrls *visited, const char *url);
int is_url_visited(const struct VisitedUrls *visited, const char *url);
void free_visited_urls(struct VisitedUrls *visited);
// Processing functions (now take VisitedUrls*)
void process_page_for_resources(CURL *curl, const char *page_url, const char *course_path, struct VisitedUrls *visited);
void extract_course_links_and_process(CURL *curl_handle, const char *html);
char* extract_course_title(const char *html);

// --- Function Implementations ---

// Initialize memory structure for libcurl callbacks
void init_memory_struct(struct MemoryStruct *chunk) {
    chunk->size = 0;
    chunk->memory = malloc(1); // Start with 1 byte, will be realloc'd
    if (chunk->memory == NULL) {
        fprintf(stderr, "DEBUG: malloc() failed in init_memory_struct\n");
        exit(EXIT_FAILURE);
    }
    chunk->memory[0] = '\0';
}

// Callback function for libcurl to write data into memory
size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "DEBUG: realloc() failed in write_memory_callback\n");
        return 0; // Signal error
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

// Callback function for libcurl to process headers (extract filename)
size_t write_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total_size = size * nitems;
    struct HeaderData *header_data = (struct HeaderData *)userdata;
    // DO NOT clear filename here, append/overwrite if found

    // Look for Content-Disposition header (case-insensitive)
    if (strncasecmp(buffer, "Content-Disposition:", 20) == 0) {
         // printf("DEBUG: Found Content-Disposition header: %s", buffer); // Print the full header line
        // Try to find filename*= first (handles UTF-8 better)
        char *filename_ptr = strstr(buffer, "filename*=");
        if (filename_ptr) {
             filename_ptr += strlen("filename*=");
             // Skip encoding part like UTF-8''
             char *encoded_value = strstr(filename_ptr, "''");
             if (encoded_value) {
                 filename_ptr = encoded_value + 2;
                 char *end_char = strpbrk(filename_ptr, "\r\n;");
                 if (end_char) {
                     size_t len = end_char - filename_ptr;
                     if (len < MAX_FILENAME_LEN) {
                         // Basic URL decoding for %XX
                         size_t out_idx = 0;
                         for (size_t i = 0; i < len && out_idx < MAX_FILENAME_LEN - 1; ++i) {
                             if (filename_ptr[i] == '%' && i + 2 < len && isxdigit((unsigned char)filename_ptr[i+1]) && isxdigit((unsigned char)filename_ptr[i+2])) {
                                 char hex[3] = {filename_ptr[i+1], filename_ptr[i+2], '\0'};
                                 header_data->filename[out_idx++] = (char)strtol(hex, NULL, 16);
                                 i += 2;
                             } else {
                                 header_data->filename[out_idx++] = filename_ptr[i];
                             }
                         }
                         header_data->filename[out_idx] = '\0';
                         // printf("--> DEBUG: Extracted filename* from header (decoded): %s\n", header_data->filename);
                         // Sanitize immediately after extraction
                         char sanitized_name[MAX_FILENAME_LEN];
                         sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                         strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                         header_data->filename[sizeof(header_data->filename)-1] = '\0';
                         // printf("--> DEBUG: Sanitized filename* from header: %s\n", header_data->filename);
                         return total_size; // Found it using filename*
                     }
                 }
             }
        }

        // If filename*= not found or failed, try plain filename=
        filename_ptr = strstr(buffer, "filename=");
        if (filename_ptr) { // Don't check header_data->filename[0] here, prioritize filename= if filename*= failed
            filename_ptr += strlen("filename=");
            // Handle quoted filenames
            if (*filename_ptr == '"') {
                filename_ptr++;
                char *end_quote = strchr(filename_ptr, '"');
                if (end_quote) {
                    size_t len = end_quote - filename_ptr;
                    if (len < MAX_FILENAME_LEN) {
                        strncpy(header_data->filename, filename_ptr, len);
                        header_data->filename[len] = '\0';
                        // printf("--> DEBUG: Extracted quoted filename= from header: %s\n", header_data->filename);
                         // Sanitize immediately
                         char sanitized_name[MAX_FILENAME_LEN];
                         sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                         strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                         header_data->filename[sizeof(header_data->filename)-1] = '\0';
                         // printf("--> DEBUG: Sanitized quoted filename= from header: %s\n", header_data->filename);
                        return total_size; // Found it using quoted filename=
                    }
                }
            } else {
                // Handle unquoted filenames (read until newline or semicolon)
                char *end_char = strpbrk(filename_ptr, "\r\n;");
                 if (end_char) {
                    size_t len = end_char - filename_ptr;
                     if (len < MAX_FILENAME_LEN) {
                        strncpy(header_data->filename, filename_ptr, len);
                        header_data->filename[len] = '\0';
                        // printf("--> DEBUG: Extracted unquoted filename= from header: %s\n", header_data->filename);
                         // Sanitize immediately
                         char sanitized_name[MAX_FILENAME_LEN];
                         sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                         strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                         header_data->filename[sizeof(header_data->filename)-1] = '\0';
                          // printf("--> DEBUG: Sanitized unquoted filename= from header: %s\n", header_data->filename);
                        return total_size; // Found it using unquoted filename=
                    }
                 }
            }
        }
    }
    return total_size; // Continue processing headers
}


// Callback function for libcurl to write data directly to a file
size_t write_data_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    if (written < nmemb && ferror(stream)) { // Check ferror on partial write
        fprintf(stderr, "DEBUG: fwrite error: %s\n", strerror(errno));
        clearerr(stream); // Clear error indicator
    }
    return written;
}


// Extract login token from HTML form
char *extract_logintoken(const char *html) {
    if (!html) {
        fprintf(stderr, "DEBUG: extract_logintoken called with NULL html\n");
        return NULL;
    }
    const char *pattern = "name=\"logintoken\" value=\"";
    const char *start = strstr(html, pattern);
    if (!start) {
         fprintf(stderr, "DEBUG: logintoken pattern not found in HTML.\n");
        return NULL;
    }
    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end) {
        fprintf(stderr, "DEBUG: Closing quote for logintoken not found.\n");
        return NULL;
    }
    size_t len = end - start;
    char *token = malloc(len + 1);
    if (!token) {
        perror("DEBUG: malloc failed for logintoken");
        return NULL;
    }
    strncpy(token, start, len);
    token[len] = '\0';
    // DEBUG: Print extracted token
    // printf("DEBUG: Extracted logintoken: %s\n", token);
    return token;
}

// Simple XOR encryption/decryption (NOT SECURE!)
void encrypt_decrypt(char *text, char key) {
    if (text == NULL) return;
    for (size_t i = 0; text[i] != '\0'; i++) {
        text[i] ^= key;
    }
}

// Securely get password input without echoing
void get_password(char *password, size_t size) {
    printf("Password: ");
    fflush(stdout);

#ifdef _WIN32
    // Windows implementation using conio.h
    int i = 0;
    char ch;
    while (i < size - 1) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') { // Enter key
            break;
        } else if (ch == '\b') { // Backspace
            if (i > 0) {
                i--;
                printf("\b \b"); // Erase character on screen
                fflush(stdout);
            }
        } else if (isprint(ch)) {
            password[i++] = ch;
            printf("*");
            fflush(stdout);
        }
    }
    password[i] = '\0';
    printf("\n");

#else
    // Unix-like implementation using termios.h
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Get current terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ECHO); // Disable echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply new settings

    if (fgets(password, size, stdin) == NULL) {
        password[0] = '\0'; // Handle read error
    } else {
        // Remove trailing newline if present
        password[strcspn(password, "\n")] = 0;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore original settings
    printf("\n"); // Print newline after password input
#endif
}


// Save encrypted credentials to file
int save_credentials(const char *username, const char *password, char key) {
    FILE *fp = fopen(CRED_FILE, "wb"); // Write in binary mode
    if (!fp) {
        perror("DEBUG: Error opening credentials file for writing");
        return 0;
    }

    // Encrypt copies before writing
    char *enc_user = strdup(username);
    char *enc_pass = strdup(password);
    if (!enc_user || !enc_pass) {
        perror("DEBUG: strdup failed in save_credentials");
        free(enc_user); // free(NULL) is safe
        free(enc_pass);
        fclose(fp);
        return 0;
    }

    encrypt_decrypt(enc_user, key);
    encrypt_decrypt(enc_pass, key);

    fprintf(fp, "%s\n", enc_user);
    fprintf(fp, "%s\n", enc_pass);

    free(enc_user);
    free(enc_pass);
    fclose(fp);
    printf("Credentials saved to %s (encrypted - basic XOR, not secure!).\n", CRED_FILE);
    return 1;
}

// Load and decrypt credentials from file
int load_credentials(char *username, size_t user_size, char *password, size_t pass_size, char key) {
    FILE *fp = fopen(CRED_FILE, "rb"); // Read in binary mode
    if (!fp) {
        // File doesn't exist or cannot be opened, which is fine on first run
        return 0;
    }

    char user_buf[256]; // Assume max length
    char pass_buf[256];

    if (fgets(user_buf, sizeof(user_buf), fp) == NULL ||
        fgets(pass_buf, sizeof(pass_buf), fp) == NULL) {
        fprintf(stderr, "DEBUG: Error reading from credentials file or file is corrupt.\n");
        fclose(fp);
        return 0; // Indicate failure
    }
    fclose(fp);

    // Remove trailing newlines
    user_buf[strcspn(user_buf, "\r\n")] = 0;
    pass_buf[strcspn(pass_buf, "\r\n")] = 0;

    // Decrypt
    encrypt_decrypt(user_buf, key);
    encrypt_decrypt(pass_buf, key);

    // Copy to output buffers if they fit
    strncpy(username, user_buf, user_size - 1);
    username[user_size - 1] = '\0'; // Ensure null termination

    strncpy(password, pass_buf, pass_size - 1);
    password[pass_size - 1] = '\0'; // Ensure null termination

    if (strlen(user_buf) >= user_size || strlen(pass_buf) >= pass_size) {
         fprintf(stderr, "DEBUG: Warning: Loaded credentials might be truncated.\n");
    }


    printf("Credentials loaded from %s.\n", CRED_FILE);
    return 1; // Indicate success
}

// Create directory if it doesn't exist
int create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
         // printf("DEBUG: Attempting to create directory: %s\n", path);
        if (MKDIR(path) != 0) {
            // Check if error is EEXIST (directory already exists, race condition?)
            if (errno == EEXIST) {
                 // printf("DEBUG: Directory already exists (race condition?): %s\n", path);
                 return 1; // Treat as success
            }
            perror("DEBUG: Error creating directory");
            fprintf(stderr, "DEBUG: Failed path: %s (errno: %d)\n", path, errno);
            return 0; // Failure
        }
        printf("Created directory: %s\n", path);
    } else {
       // Directory exists, check if it's actually a directory
       if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "DEBUG: Error: Path exists but is not a directory: %s\n", path);
            return 0; // Failure
       }
       // printf("DEBUG: Directory already exists: %s\n", path);
    }
    return 1; // Success or already exists
}

// Sanitize a string to be used as a filename/directory name
char* sanitize_filename(const char* input_filename, char* output_filename, size_t output_size) {
    if (!input_filename || !output_filename || output_size == 0) {
        if (output_filename && output_size > 0) output_filename[0] = '\0';
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; input_filename[i] != '\0' && j < output_size - 1; ++i) {
        // Allow alphanumeric, underscore, hyphen, dot, parentheses
        if (isalnum((unsigned char)input_filename[i]) || input_filename[i] == '_' || input_filename[i] == '-' || input_filename[i] == '.' || input_filename[i] == '(' || input_filename[i] == ')') {
            output_filename[j++] = input_filename[i];
        } else if (input_filename[i] == ' ' || input_filename[i] == ':') { // Replace space or colon with underscore
             output_filename[j++] = '_';
        } else if (input_filename[i] == '/') { // Replace forward slash with underscore
            output_filename[j++] = '_';
        }
        // Skip other potentially problematic characters like \, ?, *, etc.
    }
    output_filename[j] = '\0'; // Null-terminate

    // Handle empty or invalid resulting names (e.g., ".", "..")
    if (j == 0 || strcmp(output_filename, ".") == 0 || strcmp(output_filename, "..") == 0) {
        snprintf(output_filename, output_size, "default_name_%ld", (long)time(NULL));
         // printf("DEBUG: Sanitized filename resulted in empty/invalid name, using fallback: %s\n", output_filename);
    }

    return output_filename;
}


// Extract filename from the last part of a URL path
void extract_filename_from_url(const char *url, char *filename, size_t size) {
    filename[0] = '\0'; // Initialize to empty
    if (!url || size == 0) return;

    const char *last_slash = strrchr(url, '/');
    const char *name_start = last_slash ? last_slash + 1 : url;

    // Remove query parameters if any
    const char *query_start = strchr(name_start, '?');
    size_t name_len = query_start ? (size_t)(query_start - name_start) : strlen(name_start);

    if (name_len > 0 && name_len < size) {
        strncpy(filename, name_start, name_len);
        filename[name_len] = '\0';
    } else if (name_len == 0 && size > 0) {
         // If name_len is 0 (e.g., URL ends in /), use a fallback
         snprintf(filename, size, "download_%ld", (long)time(NULL));
         // printf("DEBUG: URL ended in slash or name was empty, using fallback filename: %s\n", filename);
    } else if (size > 0) {
        // Fallback filename if extraction fails or name is too long
        snprintf(filename, size, "download_%ld", (long)time(NULL));
         // printf("DEBUG: Filename extraction from URL failed or too long, using fallback: %s\n", filename);
    } else {
        return; // Size is 0
    }

    // Further sanitize the extracted name
    char sanitized_name[MAX_FILENAME_LEN];
    sanitize_filename(filename, sanitized_name, sizeof(sanitized_name));
    strncpy(filename, sanitized_name, size -1);
    filename[size - 1] = '\0';
    // printf("DEBUG: Filename extracted from URL and sanitized: %s\n", filename);
}

// Download a file from a given URL
void download_file(CURL *curl, const char *url, const char *course_path, const char* suggested_name) {
    if (!curl || !url || !course_path) return;

    printf("Attempting to download resource: %s\n", url);

    FILE *fp = NULL;
    char filepath[MAX_PATH_LEN];
    char filename[MAX_FILENAME_LEN] = {0};
    struct HeaderData header_data = {0}; // Initialize filename in header data to empty
    char final_url[MAX_URL_LEN] = {0}; // To store the final URL after redirects

    // --- Prepare cURL request ---
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    // Set callbacks and data pointers *before* perform
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback); // Write to memory first
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback); // Header callback
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data); // Pass struct to store filename

    struct MemoryStruct temp_data; // Temporary buffer for downloaded data
    init_memory_struct(&temp_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &temp_data); // Point write callback to temp buffer

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L); // Disable progress meter for downloads
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Don't fail on HTTP > 400, check code manually
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Ensure GET request
    // Clear previous error buffer
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);


    // --- Perform the request ---
    // printf("DEBUG: Performing request for headers/file...\n");
    CURLcode res = curl_easy_perform(curl);

    // Get info *after* perform
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, final_url);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // printf("DEBUG: Request finished. Result: %d (%s), HTTP Code: %ld, Effective URL: %s\n",
    //        res, curl_easy_strerror(res), http_code, final_url);


    if (res != CURLE_OK) {
         fprintf(stderr, "DEBUG: curl_easy_perform() failed for URL %s: %s\n", url, curl_easy_strerror(res));
         fprintf(stderr, "DEBUG: Curl error details: %s\n", errbuf);
         free(temp_data.memory);
         goto download_cleanup; // Skip file processing
    }

     if (http_code >= 400) {
         fprintf(stderr, "DEBUG: HTTP error %ld received for URL: %s\n", http_code, url);
         // Print response body snippet for debugging server errors
         // fprintf(stderr, "DEBUG: Server response snippet (up to 500 chars):\n%.500s\n", temp_data.memory);
         free(temp_data.memory);
         goto download_cleanup; // Skip file processing
     }


    // --- Determine filename ---
    // header_data.filename should be populated by the callback if Content-Disposition was found
    if (strlen(header_data.filename) > 0) {
         strncpy(filename, header_data.filename, sizeof(filename) - 1);
         filename[sizeof(filename) - 1] = '\0';
         printf("--> Using filename from header: %s\n", filename);
    } else if (suggested_name && strlen(suggested_name) > 0) {
        // Use suggested name if header didn't provide one
        char sanitized_suggested[MAX_FILENAME_LEN];
        sanitize_filename(suggested_name, sanitized_suggested, sizeof(sanitized_suggested));
        strncpy(filename, sanitized_suggested, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
         printf("--> Using suggested filename (sanitized): %s\n", filename);
    } else {
        // Fallback to extracting from the *final* URL
        extract_filename_from_url(final_url, filename, sizeof(filename));
        printf("--> Using filename from final URL: %s\n", filename);
    }

     // If filename is still empty after all attempts, create a generic one
     if (strlen(filename) == 0) {
        snprintf(filename, sizeof(filename), "download_%ld.unknown", (long)time(NULL));
        printf("--> WARNING: Could not determine filename, using generic: %s\n", filename);
     }


    // --- Construct full path ---
    snprintf(filepath, sizeof(filepath), "%s/%s", course_path, filename);

    // --- Check if file already exists ---
     struct stat st;
     if (stat(filepath, &st) == 0) {
         printf("File already exists, skipping: %s\n", filepath);
         free(temp_data.memory);
         goto download_cleanup; // Skip download
     }


    // --- Open file for writing ---
    // printf("DEBUG: Opening file for writing: %s\n", filepath);
    fp = fopen(filepath, "wb");
    if (!fp) {
        perror("DEBUG: Error opening file for writing");
        fprintf(stderr, "DEBUG: Failed path: %s\n", filepath);
        free(temp_data.memory);
        goto download_cleanup;
    }

    // --- Write the data (already fetched into temp_data) ---
    if (temp_data.size > 0) {
        // printf("DEBUG: Writing %zu bytes from memory to file...\n", temp_data.size);
        size_t written = fwrite(temp_data.memory, 1, temp_data.size, fp);
        if (written < temp_data.size) {
             fprintf(stderr, "DEBUG: Error writing data to file: wrote %zu of %zu bytes.\n", written, temp_data.size);
             perror("DEBUG: fwrite error");
             fclose(fp);
             remove(filepath); // Remove partial file
             free(temp_data.memory);
             goto download_cleanup;
        }
         printf("Successfully downloaded: %s\n", filepath);
    } else if (res == CURLE_OK && http_code < 400) {
        // Handle 0-byte files correctly
        // printf("DEBUG: Download successful (0 bytes): %s\n", filepath);
        // Create empty file if it doesn't exist
        fclose(fp); // Close first
        fp = NULL; // Avoid double close
        struct stat st_check;
        if (stat(filepath, &st_check) != 0) {
             // If somehow fwrite didn't create the file (unlikely for wb), try creating empty
             fp = fopen(filepath, "wb");
             if (fp) fclose(fp);
             fp = NULL;
        }
         printf("Successfully downloaded (0 bytes): %s\n", filepath);

    } else {
         // Should not happen if checks above passed, but as a fallback
         fprintf(stderr, "DEBUG: No data received or error occurred before writing.\n");
         if (fp) fclose(fp); // Close if open
         fp = NULL;
         remove(filepath); // Remove potentially empty/corrupt file
         free(temp_data.memory);
         goto download_cleanup;
    }


    if (fp) fclose(fp); // Close the file if still open
    free(temp_data.memory); // Free the temporary memory buffer

download_cleanup:
     // Reset options that might interfere with next request
    // Resetting write function/data is crucial if the same handle is reused for HTML pages
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback); // Back to memory for HTML pages
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL); // Will be set by next call needing it
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Ensure GET for subsequent page fetches
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL); // Reset error buffer pointer

}

// --- Visited URL List Functions ---

// Initialize the visited URL list
void init_visited_urls(struct VisitedUrls *visited) {
    visited->urls = malloc(INITIAL_VISITED_CAPACITY * sizeof(char*));
    if (!visited->urls) {
        perror("DEBUG: Failed to allocate initial visited URL list");
        exit(EXIT_FAILURE);
    }
    visited->count = 0;
    visited->capacity = INITIAL_VISITED_CAPACITY;
}

// Add a URL to the visited list, resizing if necessary
int add_visited_url(struct VisitedUrls *visited, const char *url) {
    // Check if capacity needs to be increased
    if (visited->count >= visited->capacity) {
        size_t new_capacity = visited->capacity * 2;
        char **new_urls = realloc(visited->urls, new_capacity * sizeof(char*));
        if (!new_urls) {
            perror("DEBUG: Failed to reallocate visited URL list");
            // Continue with current capacity, might miss preventing some loops
            return 0;
        }
        visited->urls = new_urls;
        visited->capacity = new_capacity;
        printf("DEBUG: Resized visited URL list capacity to %zu\n", new_capacity);
    }

    // Allocate memory for the new URL string and copy it
    visited->urls[visited->count] = strdup(url);
    if (!visited->urls[visited->count]) {
        perror("DEBUG: Failed to duplicate URL string for visited list");
        return 0; // Failed to add
    }
    visited->count++;
    return 1; // Success
}

// Check if a URL is already in the visited list
int is_url_visited(const struct VisitedUrls *visited, const char *url) {
    for (size_t i = 0; i < visited->count; i++) {
        if (visited->urls[i] && strcmp(visited->urls[i], url) == 0) {
            return 1; // Found
        }
    }
    return 0; // Not found
}

// Free memory allocated for the visited URL list
void free_visited_urls(struct VisitedUrls *visited) {
    if (visited->urls) {
        for (size_t i = 0; i < visited->count; i++) {
            free(visited->urls[i]); // Free each duplicated URL string
        }
        free(visited->urls); // Free the array of pointers
        visited->urls = NULL;
        visited->count = 0;
        visited->capacity = 0;
    }
}

// --- End Visited URL List Functions ---


// Fetch a page and process it for resource and folder links
void process_page_for_resources(CURL *curl, const char *page_url, const char *course_path, struct VisitedUrls *visited) {
    if (!curl || !page_url || !course_path || !visited) return;

    // --- Check if URL has already been visited ---
    if (is_url_visited(visited, page_url)) {
        printf("DEBUG: URL already processed, skipping: %s\n", page_url);
        return;
    }

    // --- Add URL to visited list BEFORE processing ---
    if (!add_visited_url(visited, page_url)) {
        fprintf(stderr, "DEBUG: Failed to add URL to visited list, cannot proceed: %s\n", page_url);
        return; // Avoid potential infinite loop if adding fails
    }
    printf("Processing page for resources: %s\n", page_url); // Moved after visited check

    CURLcode res;
    struct MemoryStruct page_content;
    init_memory_struct(&page_content); // Initialize memory buffer

    // Configure curl for fetching the page content
    curl_easy_setopt(curl, CURLOPT_URL, page_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&page_content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Ensure GET request
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Check HTTP code manually
    // Clear previous error buffer
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);


    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "DEBUG: curl_easy_perform() failed while fetching page %s: %s\n", page_url, curl_easy_strerror(res));
        fprintf(stderr, "DEBUG: Curl error details: %s\n", errbuf);
        free(page_content.memory);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL); // Reset error buffer
        // Note: URL remains in visited list even on fetch failure to prevent retries
        return;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
     // printf("DEBUG: HTTP code for page %s: %ld\n", page_url, http_code);
    if (http_code >= 400) {
         fprintf(stderr, "DEBUG: HTTP error %ld while fetching page %s\n", http_code, page_url);
         free(page_content.memory);
         curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL); // Reset error buffer
         // Note: URL remains in visited list even on HTTP error
         return;
    }
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL); // Reset error buffer


    // --- Parse the fetched HTML content ---
    // DEBUG: Print first 500 chars of the page if needed
    // printf("DEBUG: Page Content Snippet (%s):\n%.500s\n", page_url, page_content.memory);

    const char *html_ptr = page_content.memory;
    const char *base_url = "https://welearn.iiserkol.ac.in"; // Adjust if needed

    while (html_ptr != NULL && *html_ptr != '\0') { // Ensure not pointing to null terminator
        // Find the next link element "<a "
        const char *link_start = strstr(html_ptr, "<a ");
        if (!link_start) break; // No more links found

        // Find href within the <a> tag
        const char *href_start = strstr(link_start, "href=\"");
        if (!href_start) {
            html_ptr = link_start + 3; // Move past "<a "
            continue;
        }
        href_start += strlen("href=\"");
        const char *href_end = strchr(href_start, '"');
        if (!href_end) {
            html_ptr = href_start; // Move past the broken tag start
            continue;
        }

        // Extract the URL
        size_t url_len = href_end - href_start;
        char current_url[MAX_URL_LEN];
        if (url_len < sizeof(current_url) && url_len > 0) {
            strncpy(current_url, href_start, url_len);
            current_url[url_len] = '\0';
            // printf("DEBUG: Found potential link URL: %s\n", current_url);


            // Find the link text (potential filename suggestion)
            char suggested_name[MAX_FILENAME_LEN] = "";
            const char* tag_end = strchr(href_end, '>'); // Find end of <a> tag
            if (tag_end) {
                const char* text_start = tag_end + 1;
                const char* text_end = strstr(text_start, "</a>");
                if (text_end && text_start < text_end) { // Ensure text_start is before text_end
                    // Simple extraction for now, might contain nested tags
                    size_t text_len = text_end - text_start;
                    // Trim leading/trailing whitespace more carefully
                    while (text_len > 0 && isspace((unsigned char)*text_start)) {
                        text_start++;
                        text_len--;
                    }
                    while (text_len > 0 && isspace((unsigned char)text_start[text_len - 1])) {
                        text_len--;
                    }

                    // Check for inner tags like img or span and try to skip them
                    const char* inner_tag_start = strchr(text_start, '<');
                    // Only adjust if inner tag is actually within the bounds
                    if (inner_tag_start != NULL && inner_tag_start < text_start + text_len) {
                         // If the link starts with an inner tag, we might not get a useful name
                         // Try to find text after the inner tag if possible
                         const char* inner_tag_end = strchr(inner_tag_start, '>');
                         if(inner_tag_end && inner_tag_end < text_start + text_len) {
                             // Check if there's text between end of inner tag and </a>
                             if (inner_tag_end + 1 < text_end) {
                                 text_start = inner_tag_end + 1;
                                 // Recalculate length and trim again
                                 text_len = text_end - text_start;
                                  while (text_len > 0 && isspace((unsigned char)*text_start)) {
                                    text_start++;
                                    text_len--;
                                 }
                                 while (text_len > 0 && isspace((unsigned char)text_start[text_len - 1])) {
                                    text_len--;
                                 }
                             } else {
                                 // No text after inner tag
                                 text_len = 0;
                             }

                         } else {
                             // Malformed inner tag, likely no useful text
                             text_len = 0;
                         }
                    }


                    if (text_len > 0 && text_len < sizeof(suggested_name)) {
                        strncpy(suggested_name, text_start, text_len);
                        suggested_name[text_len] = '\0';
                        // printf("DEBUG: Found link text suggestion: '%s'\n", suggested_name);
                    }
                }
            }


            // Check if it's a resource link we care about
            // Added check for '#' to avoid fragment links
            if ((strstr(current_url, "/mod/resource/view.php?id=") || strstr(current_url, "/pluginfile.php/")) && current_url[0] != '#') {
                // Ensure it's a full URL
                char full_url[MAX_URL_LEN];
                 if (strncmp(current_url, "http", 4) != 0) {
                    // Handle relative URLs carefully (might be relative to base or current page)
                    // Assuming relative to base_url for now
                    snprintf(full_url, sizeof(full_url), "%s%s", base_url, current_url); // Prepend base URL if relative
                } else {
                     strncpy(full_url, current_url, sizeof(full_url)-1);
                     full_url[sizeof(full_url)-1] = '\0';
                }
                 // printf("DEBUG: Identified resource link: %s (Suggested name: '%s')\n", full_url, suggested_name);
                 download_file(curl, full_url, course_path, suggested_name);
                 SLEEP(1); // Add a small delay between downloads
            }
            // Check if it's a folder link
            else if (strstr(current_url, "/mod/folder/view.php?id=") && current_url[0] != '#') {
                 // Ensure it's a full URL
                char full_url[MAX_URL_LEN];
                 if (strncmp(current_url, "http", 4) != 0) {
                    snprintf(full_url, sizeof(full_url), "%s%s", base_url, current_url);
                } else {
                    strncpy(full_url, current_url, sizeof(full_url)-1);
                    full_url[sizeof(full_url)-1] = '\0';
                }
                // Recursively process the folder page, passing the visited list
                printf("--- Entering Folder: %s ---\n", full_url);
                process_page_for_resources(curl, full_url, course_path, visited); // Recursive call with visited list
                 printf("--- Exiting Folder: %s ---\n", full_url);
                 SLEEP(1); // Add a small delay
            }
        }
        // Move pointer past the current link's end quote to search for the next one
        html_ptr = href_end + 1; // Move past the quote
    }

    // Free the memory used for the page content
    free(page_content.memory);
}

// Extract course title from HTML <title> tag
char* extract_course_title(const char *html) {
    if (!html) {
         fprintf(stderr, "DEBUG: extract_course_title called with NULL html\n");
        return NULL;
    }

    const char *title_start_tag = "<title>";
    const char *title_end_tag = "</title>";

    const char *start = strstr(html, title_start_tag);
    if (!start) {
        fprintf(stderr, "DEBUG: <title> tag start not found.\n");
        return NULL;
    }
    start += strlen(title_start_tag);

    const char *end = strstr(start, title_end_tag);
    if (!end) {
        fprintf(stderr, "DEBUG: </title> tag end not found.\n");
        return NULL;
    }

    // Try to extract the most specific part of the title
    // Look for "Course: " prefix first
    const char *course_prefix = "Course: ";
    if (strncmp(start, course_prefix, strlen(course_prefix)) == 0) {
        start += strlen(course_prefix);
    }
    // Moodle often includes the site name at the end, e.g., "My Course Name : Site Name"
    // Try to remove the site name part if present (assuming it's after a colon)
    // This is heuristic and might need adjustment
    const char *site_name_separator = " : "; // Check for space-colon-space
    const char *site_sep_pos = strstr(start, site_name_separator);
     if (site_sep_pos != NULL && site_sep_pos < end) {
         // Use the part before the separator as the title
         end = site_sep_pos;
     }


    size_t len = end - start;
    if (len == 0) {
         fprintf(stderr, "DEBUG: Extracted title length is zero.\n");
         return NULL;
    }

    char *title = malloc(len + 1);
    if (!title) {
        perror("DEBUG: malloc failed for course title");
        return NULL;
    }
    strncpy(title, start, len);
    title[len] = '\0';

    // Trim leading/trailing whitespace
    char *trimmed_start = title;
    while (isspace((unsigned char)*trimmed_start)) {
        trimmed_start++;
    }
    if (*trimmed_start == 0) { // String is all whitespace
        free(title);
        fprintf(stderr, "DEBUG: Extracted title was all whitespace after trimming.\n");
        return NULL;
    }

    char *trimmed_end = trimmed_start + strlen(trimmed_start) - 1;
    while (trimmed_end > trimmed_start && isspace((unsigned char)*trimmed_end)) {
        trimmed_end--;
    }
    *(trimmed_end + 1) = '\0'; // Write null terminator


    // Sanitize for directory name
    char sanitized_title[MAX_PATH_LEN];
    sanitize_filename(trimmed_start, sanitized_title, sizeof(sanitized_title));

    free(title); // Free original allocated title buffer

    // printf("DEBUG: Extracted and sanitized course title: %s\n", sanitized_title);
    // Ensure sanitized title is not empty
    if (strlen(sanitized_title) == 0) {
        fprintf(stderr, "DEBUG: Sanitized title is empty.\n");
        return NULL;
    }
    return strdup(sanitized_title); // Return sanitized copy
}


// Extract course links from the main dashboard page and process each one
void extract_course_links_and_process(CURL *curl_handle, const char *html) {
    if (!html || !curl_handle) return;

    printf("\n--- Extracting and Processing Course Links ---\n");

    // --- Initialize Visited URL List ---
    struct VisitedUrls visited_list;
    init_visited_urls(&visited_list);


    // --- Start searching from the 'My courses' section for more accuracy ---
    const char *mycourses_marker = "data-key=\"mycourses\"";
    const char *search_start_ptr = strstr(html, mycourses_marker);
    const char *html_ptr = NULL; // Initialize search pointer

    if (!search_start_ptr) {
        fprintf(stderr, "DEBUG: Could not find the 'My courses' marker ('%s') in the dashboard HTML. Searching from beginning.\n", mycourses_marker);
        html_ptr = html; // Fallback to searching the whole page
    } else {
         printf("DEBUG: Found 'My courses' marker. Starting search for course links from this point.\n");
         html_ptr = search_start_ptr + strlen(mycourses_marker); // Start search *after* the marker text
    }

    // --- Define the specific pattern for course links ---
    // Based on HTML: <a class="list-group-item list-group-item-action " href="[URL]">
    const char *specific_link_tag_start = "<a class=\"list-group-item list-group-item-action \" href=\"";
    const char *course_url_pattern = "/course/view.php?id="; // Still used to verify the extracted URL
    const char *base_url = "https://welearn.iiserkol.ac.in"; // Adjust if needed
    int found_courses = 0;


    while (html_ptr != NULL && *html_ptr != '\0') {
        // Find the start of the specific link tag
        const char *link_tag_start = strstr(html_ptr, specific_link_tag_start);
        if (!link_tag_start) break; // No more matching links found

        // Move pointer to the start of the URL value
        const char *link_start = link_tag_start + strlen(specific_link_tag_start);

        // Find the closing quote for the href
        const char *link_end = strchr(link_start, '"');
        if (!link_end) {
            html_ptr = link_start; // Move past broken tag start
            continue;
        }

        // Extract the URL itself
        size_t url_len = link_end - link_start;
        char current_url[MAX_URL_LEN];
        if (url_len < sizeof(current_url) && url_len > 0) {
             strncpy(current_url, link_start, url_len);
             current_url[url_len] = '\0';

             // Now check if this specifically found URL contains the course pattern
            if (strstr(current_url, course_url_pattern)) {
                 found_courses++;
                 // printf("DEBUG: Found specific course link tag (#%d): %s\n", found_courses, current_url);

                 // Construct full URL (should already be absolute based on HTML)
                char full_course_url[MAX_URL_LEN];
                 if (strncmp(current_url, "http", 4) != 0) {
                     printf("DEBUG: Warning - Course link seems relative: %s. Prepending base URL.\n", current_url);
                    snprintf(full_course_url, sizeof(full_course_url), "%s%s", base_url, current_url);
                } else {
                     strncpy(full_course_url, current_url, sizeof(full_course_url) - 1);
                     full_course_url[sizeof(full_course_url) - 1] = '\0';
                }

                printf("\nFound Course Link: %s\n", full_course_url);

                // --- Fetch the course page to get title and resources ---
                CURLcode res;
                struct MemoryStruct course_page_content;
                init_memory_struct(&course_page_content);

                // printf("DEBUG: Fetching course page content for %s...\n", full_course_url);
                curl_easy_setopt(curl_handle, CURLOPT_URL, full_course_url);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&course_page_content);
                curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L); // Ensure GET
                curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 0L); // Check HTTP code manually
                // Clear previous error buffer
                char errbuf_course[CURL_ERROR_SIZE] = {0};
                curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf_course);


                res = curl_easy_perform(curl_handle);

                if (res == CURLE_OK) {
                    long http_code = 0;
                    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
                     // printf("DEBUG: HTTP code for course page %s: %ld\n", full_course_url, http_code);
                    if (http_code < 400) {
                        // Extract course title
                         // printf("DEBUG: Attempting to extract course title...\n");
                        char *course_title = extract_course_title(course_page_content.memory);
                        if (course_title && strlen(course_title) > 0) {
                            printf("Processing Course: %s\n", course_title);

                            // Create directory for the course
                            char course_path[MAX_PATH_LEN];
                            snprintf(course_path, sizeof(course_path), "./%s", course_title); // Create in current dir
                            if (create_directory(course_path)) {
                                 // printf("DEBUG: Calling process_page_for_resources for %s in path %s\n", full_course_url, course_path);
                                // Process this course page for resources and folders, passing visited list
                                process_page_for_resources(curl_handle, full_course_url, course_path, &visited_list);
                            } else {
                                fprintf(stderr, "DEBUG: Failed to create directory for course: %s (Path: %s)\n", course_title, course_path);
                            }
                            free(course_title);
                        } else {
                            fprintf(stderr, "DEBUG: Could not extract a valid title for course: %s\n", full_course_url);
                            // Create a default directory name based on URL id
                            const char* id_param = "?id=";
                            const char* id_start = strstr(full_course_url, id_param);
                            char default_dir_name[64] = "course_unknown"; // Default fallback
                            if (id_start){
                                id_start += strlen(id_param);
                                // Extract only digits for the ID part
                                char id_str[16];
                                int i = 0;
                                while(isdigit((unsigned char)id_start[i]) && i < sizeof(id_str) - 1) {
                                    id_str[i] = id_start[i];
                                    i++;
                                }
                                id_str[i] = '\0';
                                if (i > 0) { // Check if we extracted any digits
                                     snprintf(default_dir_name, sizeof(default_dir_name), "course_%s", id_str);
                                }
                            }
                            char sanitized_default_name[MAX_PATH_LEN];
                            sanitize_filename(default_dir_name, sanitized_default_name, sizeof(sanitized_default_name));
                            printf("DEBUG: Using default directory name: %s\n", sanitized_default_name);

                            char course_path[MAX_PATH_LEN];
                            snprintf(course_path, sizeof(course_path), "./%s", sanitized_default_name);
                            if (create_directory(course_path)) {
                                 // printf("DEBUG: Calling process_page_for_resources for %s in default path %s\n", full_course_url, course_path);
                                 process_page_for_resources(curl_handle, full_course_url, course_path, &visited_list);
                            } else {
                                 fprintf(stderr, "DEBUG: Failed to create default directory: %s\n", course_path);
                            }

                        }
                    } else {
                         fprintf(stderr, "DEBUG: HTTP error %ld fetching course page: %s\n", http_code, full_course_url);
                    }
                } else {
                    fprintf(stderr, "DEBUG: curl_easy_perform() failed for course page %s: %s\n", full_course_url, curl_easy_strerror(res));
                     fprintf(stderr, "DEBUG: Curl error details: %s\n", errbuf_course);
                }
                curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, NULL); // Reset error buffer

                free(course_page_content.memory); // Free memory for this course page
                SLEEP(2); // Be polite, wait between processing courses
            } // End if (strstr(current_url, course_url_pattern))
        } // End if (url_len < sizeof(current_url) && url_len > 0)

         // Move pointer past the end quote of the current link tag to find the next one
        html_ptr = link_end + 1; // Move pointer past the quote
    } // End while loop

    if (found_courses == 0) {
        printf("DEBUG: No course links matching the specific pattern ('%s' containing '%s') were found after the 'My courses' marker.\n", specific_link_tag_start, course_url_pattern);
        if (search_start_ptr == html) { // Add this check if fallback was used
             printf("DEBUG: Also searched from the beginning of the page.\n");
        }
    } else {
         printf("DEBUG: Found and initiated processing for %d course links.\n", found_courses);
    }

     printf("\n--- Finished Processing Course Links ---\n");

     // --- Clean up Visited URL List ---
     free_visited_urls(&visited_list);
}


// --- Main Function ---
int main(void) {
    CURL *curl;
    CURLcode res;
    char username[128];
    char password[128];
    char errbuf[CURL_ERROR_SIZE] = {0}; // Initialize error buffer

    // --- Initialize libcurl ---
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return EXIT_FAILURE;
    }

     // --- Set common cURL options ---
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt"); // Save cookies
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt"); // Load cookies
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"); // Behave like a browser
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificate - SET TO 0L TO DISABLE (NOT RECOMMENDED)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // Verify hostname in cert - SET TO 0L TO DISABLE (NOT RECOMMENDED)
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Uncomment for very detailed curl output
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf); // Store errors


    // --- Handle Credentials ---
    if (!load_credentials(username, sizeof(username), password, sizeof(password), ENCRYPTION_KEY)) {
        printf("Credentials not found or failed to load.\nPlease enter your WeLearn credentials:\n");
        printf("Username: ");
        fflush(stdout);
        if (fgets(username, sizeof(username), stdin) == NULL) {
             fprintf(stderr, "Error reading username.\n");
             goto cleanup;
        }
        username[strcspn(username, "\n")] = 0; // Remove newline

        get_password(password, sizeof(password));

        if (strlen(username) == 0 || strlen(password) == 0) {
             fprintf(stderr, "Username or password cannot be empty.\n");
             goto cleanup;
        }

        // Ask to save credentials
        printf("Save credentials for future use? (y/n): ");
        fflush(stdout);
        int choice_char = getchar(); // Read char
        char choice = (char)choice_char;
         // Consume potential leftover newline character(s) from input buffer
        int c;
        while ((c = getchar()) != '\n' && c != EOF);


        if (choice == 'y' || choice == 'Y') {
            if (!save_credentials(username, password, ENCRYPTION_KEY)) {
                 fprintf(stderr, "Warning: Failed to save credentials.\n");
            }
        }
    } else {
        printf("Using saved credentials for user: %s\n", username);
    }


    // --- Step 1: Get Login Page to Extract Token ---
    const char *login_url = "https://welearn.iiserkol.ac.in/login/index.php";
    struct MemoryStruct login_page_content;
    init_memory_struct(&login_page_content);

    curl_easy_setopt(curl, CURLOPT_URL, login_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Ensure GET request
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Check HTTP code manually

    printf("Fetching login page to get token...\n");
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed for login page: %s\n", curl_easy_strerror(res));
        fprintf(stderr, "Curl error details: %s\n", errbuf);
        free(login_page_content.memory);
        goto cleanup;
    }
    long http_code_login_page = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code_login_page);
    if (http_code_login_page >= 400) {
         fprintf(stderr, "HTTP error %ld fetching login page.\n", http_code_login_page);
         free(login_page_content.memory);
         goto cleanup;
    }


    char *logintoken = extract_logintoken(login_page_content.memory);
    if (!logintoken) {
        fprintf(stderr, "Failed to extract logintoken. Check if login page structure changed.\n");
        // Optionally print more of login_page_content.memory for debugging
        // printf("DEBUG: Login Page Content (first 500 chars):\n%.500s\n", login_page_content.memory);
        free(login_page_content.memory);
        goto cleanup;
    }
    printf("Login token extracted successfully.\n");


    // --- Step 2: Perform Login ---
    // URL encode username and password
    char *escaped_username = curl_easy_escape(curl, username, 0);
    char *escaped_password = curl_easy_escape(curl, password, 0);
    if (!escaped_username || !escaped_password) {
        fprintf(stderr, "Failed to URL-encode credentials\n");
        free(login_page_content.memory);
        free(logintoken);
        goto cleanup;
    }

    // Prepare POST fields
    char post_fields[1024];
    snprintf(post_fields, sizeof(post_fields), "username=%s&password=%s&logintoken=%s&anchor=",
             escaped_username, escaped_password, logintoken);

    // Free memory from previous request (login page), re-init for login response (dashboard)
    free(login_page_content.memory);
    init_memory_struct(&login_page_content); // Re-initialize for dashboard content

    // Set POST options
    curl_easy_setopt(curl, CURLOPT_URL, login_url); // Post back to the same URL
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback); // Write response to memory
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(curl, CURLOPT_REFERER, login_url); // Set referer
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Check HTTP code manually


    printf("Attempting login...\n");
    // Reset error buffer before perform
    errbuf[0] = '\0';
    res = curl_easy_perform(curl);

    // Clean up escaped strings and token now that they are used
    curl_free(escaped_username);
    curl_free(escaped_password);
    free(logintoken);
    logintoken = NULL; // Avoid double free

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed during login POST: %s\n", curl_easy_strerror(res));
         fprintf(stderr, "Curl error details: %s\n", errbuf);
        free(login_page_content.memory);
        goto cleanup;
    }

    long http_code_login_post = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code_login_post);
     // printf("DEBUG: HTTP code after login POST: %ld\n", http_code_login_post);
     // Note: Successful login often results in a redirect (30x),
     // but CURLOPT_FOLLOWLOCATION handles this, so we expect 200 on the final page.


    // --- Step 3: Check Login Success & Process Dashboard ---
    // Check if login was successful by looking for indicators on the dashboard page.
    // A common indicator is the presence of a "Logout" link or the user's name.
    // Conversely, check for login error messages.
    if (strstr(login_page_content.memory, "Invalid login, please try again") || strstr(login_page_content.memory, "loginerrors")) {
        fprintf(stderr, "Login failed! Please check your username and password.\n");
        // fprintf(stderr, "DEBUG: Dashboard content snippet on login failure:\n%.500s\n", login_page_content.memory);
        // Optionally delete the invalid saved credentials
        // remove(CRED_FILE);
        free(login_page_content.memory);
        goto cleanup;
    }
    // Add a positive check if possible, e.g., finding the logout link
    if (!strstr(login_page_content.memory, "/login/logout.php")) {
         fprintf(stderr, "Warning: Login might have failed - Logout link not found on the resulting page.\n");
         // Consider treating this as an error depending on site behavior
         // free(login_page_content.memory);
         // goto cleanup;
    }


    printf("Login successful!\n");

    // Now, the login_page_content.memory should contain the dashboard HTML
    // Extract and process course links from the dashboard
    extract_course_links_and_process(curl, login_page_content.memory);

    // Free the dashboard page content
    free(login_page_content.memory);


    // --- Final Cleanup ---
cleanup:
    if (logintoken) free(logintoken); // Free if allocated and not freed earlier
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    printf("\nProgram finished.\n");
    return EXIT_SUCCESS;
}
