#ifndef WELEARN_DOWNLOAD_H
#define WELEARN_DOWNLOAD_H

#include "welearn_common.h"

// Download functions
void download_file(CURL *curl, const char *url, const char *course_path, const char* suggested_name);
void process_page_for_resources(CURL *curl, const char *page_url, const char *course_path, struct VisitedUrls *visited);
char* extract_course_title(const char *html);
void extract_course_links_and_process(CURL *curl_handle, const char *html);

#endif // WELEARN_DOWNLOAD_H
