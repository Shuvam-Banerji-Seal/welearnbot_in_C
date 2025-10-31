#include "../include/welearn_common.h"
#include <errno.h>
#include <ctype.h>
#include <time.h>

// Initialize memory structure for libcurl callbacks
void init_memory_struct(struct MemoryStruct *chunk) {
    chunk->size = 0;
    chunk->memory = malloc(1);
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
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

// Callback function for libcurl to process headers
size_t write_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    size_t total_size = size * nitems;
    struct HeaderData *header_data = (struct HeaderData *)userdata;

    if (strncasecmp(buffer, "Content-Disposition:", 20) == 0) {
        char *filename_ptr = strstr(buffer, "filename*=");
        if (filename_ptr) {
            filename_ptr += strlen("filename*=");
            char *encoded_value = strstr(filename_ptr, "''");
            if (encoded_value) {
                filename_ptr = encoded_value + 2;
                char *end_char = strpbrk(filename_ptr, "\r\n;");
                if (end_char) {
                    size_t len = end_char - filename_ptr;
                    if (len < MAX_FILENAME_LEN) {
                        size_t out_idx = 0;
                        for (size_t i = 0; i < len && out_idx < MAX_FILENAME_LEN - 1; ++i) {
                            if (filename_ptr[i] == '%' && i + 2 < len && 
                                isxdigit((unsigned char)filename_ptr[i+1]) && 
                                isxdigit((unsigned char)filename_ptr[i+2])) {
                                char hex[3] = {filename_ptr[i+1], filename_ptr[i+2], '\0'};
                                header_data->filename[out_idx++] = (char)strtol(hex, NULL, 16);
                                i += 2;
                            } else {
                                header_data->filename[out_idx++] = filename_ptr[i];
                            }
                        }
                        header_data->filename[out_idx] = '\0';
                        char sanitized_name[MAX_FILENAME_LEN];
                        sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                        strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                        header_data->filename[sizeof(header_data->filename)-1] = '\0';
                        return total_size;
                    }
                }
            }
        }

        filename_ptr = strstr(buffer, "filename=");
        if (filename_ptr) {
            filename_ptr += strlen("filename=");
            if (*filename_ptr == '"') {
                filename_ptr++;
                char *end_quote = strchr(filename_ptr, '"');
                if (end_quote) {
                    size_t len = end_quote - filename_ptr;
                    if (len < MAX_FILENAME_LEN) {
                        strncpy(header_data->filename, filename_ptr, len);
                        header_data->filename[len] = '\0';
                        char sanitized_name[MAX_FILENAME_LEN];
                        sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                        strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                        header_data->filename[sizeof(header_data->filename)-1] = '\0';
                        return total_size;
                    }
                }
            } else {
                char *end_char = strpbrk(filename_ptr, "\r\n;");
                if (end_char) {
                    size_t len = end_char - filename_ptr;
                    if (len < MAX_FILENAME_LEN) {
                        strncpy(header_data->filename, filename_ptr, len);
                        header_data->filename[len] = '\0';
                        char sanitized_name[MAX_FILENAME_LEN];
                        sanitize_filename(header_data->filename, sanitized_name, sizeof(sanitized_name));
                        strncpy(header_data->filename, sanitized_name, sizeof(header_data->filename)-1);
                        header_data->filename[sizeof(header_data->filename)-1] = '\0';
                        return total_size;
                    }
                }
            }
        }
    }
    return total_size;
}

// Callback function for libcurl to write data directly to a file
size_t write_data_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    if (written < nmemb && ferror(stream)) {
        fprintf(stderr, "DEBUG: fwrite error: %s\n", strerror(errno));
        clearerr(stream);
    }
    return written;
}

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

// Add a URL to the visited list
int add_visited_url(struct VisitedUrls *visited, const char *url) {
    if (visited->count >= visited->capacity) {
        size_t new_capacity = visited->capacity * 2;
        char **new_urls = realloc(visited->urls, new_capacity * sizeof(char*));
        if (!new_urls) {
            perror("DEBUG: Failed to reallocate visited URL list");
            return 0;
        }
        visited->urls = new_urls;
        visited->capacity = new_capacity;
    }

    visited->urls[visited->count] = strdup(url);
    if (!visited->urls[visited->count]) {
        perror("DEBUG: Failed to duplicate URL string for visited list");
        return 0;
    }
    visited->count++;
    return 1;
}

// Check if a URL is already in the visited list
int is_url_visited(const struct VisitedUrls *visited, const char *url) {
    for (size_t i = 0; i < visited->count; i++) {
        if (visited->urls[i] && strcmp(visited->urls[i], url) == 0) {
            return 1;
        }
    }
    return 0;
}

// Free memory allocated for the visited URL list
void free_visited_urls(struct VisitedUrls *visited) {
    if (visited->urls) {
        for (size_t i = 0; i < visited->count; i++) {
            free(visited->urls[i]);
        }
        free(visited->urls);
        visited->urls = NULL;
        visited->count = 0;
        visited->capacity = 0;
    }
}

// Sanitize a string to be used as a filename/directory name
char* sanitize_filename(const char* input_filename, char* output_filename, size_t output_size) {
    if (!input_filename || !output_filename || output_size == 0) {
        if (output_filename && output_size > 0) output_filename[0] = '\0';
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; input_filename[i] != '\0' && j < output_size - 1; ++i) {
        if (isalnum((unsigned char)input_filename[i]) || input_filename[i] == '_' || 
            input_filename[i] == '-' || input_filename[i] == '.' || 
            input_filename[i] == '(' || input_filename[i] == ')') {
            output_filename[j++] = input_filename[i];
        } else if (input_filename[i] == ' ' || input_filename[i] == ':') {
            output_filename[j++] = '_';
        } else if (input_filename[i] == '/') {
            output_filename[j++] = '_';
        }
    }
    output_filename[j] = '\0';

    if (j == 0 || strcmp(output_filename, ".") == 0 || strcmp(output_filename, "..") == 0) {
        snprintf(output_filename, output_size, "default_name_%ld", (long)time(NULL));
    }

    return output_filename;
}

// Extract filename from the last part of a URL path
void extract_filename_from_url(const char *url, char *filename, size_t size) {
    filename[0] = '\0';
    if (!url || size == 0) return;

    const char *last_slash = strrchr(url, '/');
    const char *name_start = last_slash ? last_slash + 1 : url;

    const char *query_start = strchr(name_start, '?');
    size_t name_len = query_start ? (size_t)(query_start - name_start) : strlen(name_start);

    if (name_len > 0 && name_len < size) {
        strncpy(filename, name_start, name_len);
        filename[name_len] = '\0';
    } else if (name_len == 0 && size > 0) {
        snprintf(filename, size, "download_%ld", (long)time(NULL));
    } else if (size > 0) {
        snprintf(filename, size, "download_%ld", (long)time(NULL));
    } else {
        return;
    }

    char sanitized_name[MAX_FILENAME_LEN];
    sanitize_filename(filename, sanitized_name, sizeof(sanitized_name));
    strncpy(filename, sanitized_name, size -1);
    filename[size - 1] = '\0';
}

// Create directory if it doesn't exist
int create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (MKDIR(path) != 0) {
            if (errno == EEXIST) {
                return 1;
            }
            perror("DEBUG: Error creating directory");
            fprintf(stderr, "DEBUG: Failed path: %s (errno: %d)\n", path, errno);
            return 0;
        }
        printf("Created directory: %s\n", path);
    } else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "DEBUG: Error: Path exists but is not a directory: %s\n", path);
            return 0;
        }
    }
    return 1;
}

// Initialize file list
void init_file_list(struct FileList *list) {
    if (!list) return;
    list->files = malloc(INITIAL_FILE_LIST_CAPACITY * sizeof(struct FileInfo));
    if (!list->files) {
        perror("Failed to allocate file list");
        exit(EXIT_FAILURE);
    }
    list->count = 0;
    list->capacity = INITIAL_FILE_LIST_CAPACITY;
}

// Add a file to the list
int add_file_to_list(struct FileList *list, const char *filename, const char *url, 
                     const char *course_name, const char *suggested_name, int is_folder, int depth) {
    if (!list || !filename || !url || !course_name) return 0;
    
    // Resize if necessary
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        struct FileInfo *new_files = realloc(list->files, new_capacity * sizeof(struct FileInfo));
        if (!new_files) {
            perror("Failed to reallocate file list");
            return 0;
        }
        list->files = new_files;
        list->capacity = new_capacity;
    }
    
    // Add the file
    struct FileInfo *file = &list->files[list->count];
    strncpy(file->filename, filename, MAX_FILENAME_LEN - 1);
    file->filename[MAX_FILENAME_LEN - 1] = '\0';
    
    strncpy(file->url, url, MAX_URL_LEN - 1);
    file->url[MAX_URL_LEN - 1] = '\0';
    
    strncpy(file->course_name, course_name, MAX_FILENAME_LEN - 1);
    file->course_name[MAX_FILENAME_LEN - 1] = '\0';
    
    if (suggested_name) {
        strncpy(file->suggested_name, suggested_name, MAX_FILENAME_LEN - 1);
        file->suggested_name[MAX_FILENAME_LEN - 1] = '\0';
    } else {
        file->suggested_name[0] = '\0';
    }
    
    file->is_folder = is_folder;
    file->depth = depth;
    list->count++;
    
    return 1;
}

// Free file list
void free_file_list(struct FileList *list) {
    if (list && list->files) {
        free(list->files);
        list->files = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

// Display files in tree format
void display_file_tree(const struct FileList *list) {
    if (!list || list->count == 0) {
        printf("No files found.\n");
        return;
    }
    
    printf("\n========================================\n");
    printf("Files Available for Download (Tree View)\n");
    printf("========================================\n");
    
    const char *current_course = "";
    for (size_t i = 0; i < list->count; i++) {
        const struct FileInfo *file = &list->files[i];
        
        // Print course header if it changes
        if (strcmp(current_course, file->course_name) != 0) {
            current_course = file->course_name;
            printf("\n📚 Course: %s\n", current_course);
        }
        
        // Print indentation based on depth
        for (int j = 0; j < file->depth; j++) {
            printf("  ");
        }
        
        // Print file/folder icon and name
        if (file->is_folder) {
            printf("📁 [%zu] %s (folder)\n", i + 1, file->filename);
        } else {
            printf("📄 [%zu] %s\n", i + 1, file->filename);
        }
    }
    printf("\n========================================\n");
}

// Display files in simple list format
void display_file_list(const struct FileList *list) {
    if (!list || list->count == 0) {
        printf("No files found.\n");
        return;
    }
    
    printf("\n========================================\n");
    printf("Files Available for Download (List View)\n");
    printf("========================================\n");
    printf("%-5s %-30s %-40s\n", "No.", "Course", "Filename");
    printf("----------------------------------------\n");
    
    for (size_t i = 0; i < list->count; i++) {
        const struct FileInfo *file = &list->files[i];
        const char *type = file->is_folder ? " (folder)" : "";
        printf("[%-3zu] %-30.30s %-40.40s%s\n", i + 1, file->course_name, file->filename, type);
    }
    printf("========================================\n");
    printf("Total: %zu file(s)\n", list->count);
}
