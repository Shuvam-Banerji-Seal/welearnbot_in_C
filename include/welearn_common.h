#ifndef WELEARN_COMMON_H
#define WELEARN_COMMON_H

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define MAX_PATH_LEN 1024
#define MAX_URL_LEN 2048
#define MAX_FILENAME_LEN 256
#define CRED_FILE "credentials.dat"
#define ENCRYPTION_KEY 'S'
#define INITIAL_VISITED_CAPACITY 50

// Cross-platform definitions
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define MKDIR(path) _mkdir(path)
#define SLEEP(seconds) Sleep(seconds * 1000)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>
#define MKDIR(path) mkdir(path, 0777)
#define SLEEP(seconds) sleep(seconds)
#endif

// Memory structure for CURL callbacks
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Header data structure
struct HeaderData {
    char filename[MAX_FILENAME_LEN];
};

// Visited URLs tracking
struct VisitedUrls {
    char **urls;
    size_t count;
    size_t capacity;
};

// Function declarations - memory management
void init_memory_struct(struct MemoryStruct *chunk);
size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);
size_t write_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
size_t write_data_callback(void *ptr, size_t size, size_t nmemb, FILE *stream);

// Function declarations - visited URLs
void init_visited_urls(struct VisitedUrls *visited);
int add_visited_url(struct VisitedUrls *visited, const char *url);
int is_url_visited(const struct VisitedUrls *visited, const char *url);
void free_visited_urls(struct VisitedUrls *visited);

// Function declarations - utilities
char* sanitize_filename(const char* input_filename, char* output_filename, size_t output_size);
void extract_filename_from_url(const char *url, char *filename, size_t size);
int create_directory(const char *path);

#endif // WELEARN_COMMON_H
