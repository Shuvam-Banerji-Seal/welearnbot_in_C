#ifndef WELEARN_DOWNLOAD_H
#define WELEARN_DOWNLOAD_H

#include "welearn_common.h"

// Download functions
void download_file(CURL *curl, const char *url, const char *course_path, const char* suggested_name);
void process_page_for_resources(CURL *curl, const char *page_url, const char *course_path, struct VisitedUrls *visited);
char* extract_course_title(const char *html);
void extract_course_links_and_process(CURL *curl_handle, const char *html);

// New scanning functions for collecting files
void collect_page_resources(CURL *curl, const char *page_url, const char *course_name, 
                           struct VisitedUrls *visited, struct FileList *file_list, int depth);
void scan_courses_and_collect_files(CURL *curl_handle, const char *html, struct FileList *file_list);

// Interactive download functions
void download_selected_files(CURL *curl, const struct FileList *list, const int *selections, 
                            size_t selection_count, const char *base_path);

#endif // WELEARN_DOWNLOAD_H
