#include "../include/welearn_download.h"
#include "../include/welearn_auth.h"
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

// Download a file from a given URL
void download_file(CURL *curl, const char *url, const char *course_path, const char* suggested_name) {
    if (!curl || !url || !course_path) return;

    printf("Attempting to download resource: %s\n", url);

    FILE *fp = NULL;
    char filepath[MAX_PATH_LEN];
    char filename[MAX_FILENAME_LEN] = {0};
    struct HeaderData header_data = {0};
    char final_url[MAX_URL_LEN] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data);

    struct MemoryStruct temp_data;
    init_memory_struct(&temp_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &temp_data);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, final_url);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK) {
        fprintf(stderr, "DEBUG: curl_easy_perform() failed for URL %s: %s\n", url, curl_easy_strerror(res));
        fprintf(stderr, "DEBUG: Curl error details: %s\n", errbuf);
        free(temp_data.memory);
        goto download_cleanup;
    }

    if (http_code >= 400) {
        fprintf(stderr, "DEBUG: HTTP error %ld received for URL: %s\n", http_code, url);
        free(temp_data.memory);
        goto download_cleanup;
    }

    // Determine filename
    if (strlen(header_data.filename) > 0) {
        strncpy(filename, header_data.filename, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        printf("--> Using filename from header: %s\n", filename);
    } else if (suggested_name && strlen(suggested_name) > 0) {
        char sanitized_suggested[MAX_FILENAME_LEN];
        sanitize_filename(suggested_name, sanitized_suggested, sizeof(sanitized_suggested));
        strncpy(filename, sanitized_suggested, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        printf("--> Using suggested filename (sanitized): %s\n", filename);
    } else {
        extract_filename_from_url(final_url, filename, sizeof(filename));
        printf("--> Using filename from final URL: %s\n", filename);
    }

    if (strlen(filename) == 0) {
        snprintf(filename, sizeof(filename), "download_%ld.unknown", (long)time(NULL));
        printf("--> WARNING: Could not determine filename, using generic: %s\n", filename);
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", course_path, filename);

    struct stat st;
    if (stat(filepath, &st) == 0) {
        printf("File already exists, skipping: %s\n", filepath);
        free(temp_data.memory);
        goto download_cleanup;
    }

    fp = fopen(filepath, "wb");
    if (!fp) {
        perror("DEBUG: Error opening file for writing");
        fprintf(stderr, "DEBUG: Failed path: %s\n", filepath);
        free(temp_data.memory);
        goto download_cleanup;
    }

    if (temp_data.size > 0) {
        size_t written = fwrite(temp_data.memory, 1, temp_data.size, fp);
        if (written < temp_data.size) {
            fprintf(stderr, "DEBUG: Error writing data to file: wrote %zu of %zu bytes.\n", written, temp_data.size);
            perror("DEBUG: fwrite error");
            fclose(fp);
            remove(filepath);
            free(temp_data.memory);
            goto download_cleanup;
        }
        printf("Successfully downloaded: %s\n", filepath);
    } else if (res == CURLE_OK && http_code < 400) {
        fclose(fp);
        fp = NULL;
        struct stat st_check;
        if (stat(filepath, &st_check) != 0) {
            fp = fopen(filepath, "wb");
            if (fp) fclose(fp);
            fp = NULL;
        }
        printf("Successfully downloaded (0 bytes): %s\n", filepath);
    } else {
        fprintf(stderr, "DEBUG: No data received or error occurred before writing.\n");
        if (fp) fclose(fp);
        fp = NULL;
        remove(filepath);
        free(temp_data.memory);
        goto download_cleanup;
    }

    if (fp) fclose(fp);
    free(temp_data.memory);

download_cleanup:
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL);
}

// Fetch a page and process it for resource and folder links
void process_page_for_resources(CURL *curl, const char *page_url, const char *course_path, struct VisitedUrls *visited) {
    if (!curl || !page_url || !course_path || !visited) return;

    if (is_url_visited(visited, page_url)) {
        printf("DEBUG: URL already processed, skipping: %s\n", page_url);
        return;
    }

    if (!add_visited_url(visited, page_url)) {
        fprintf(stderr, "DEBUG: Failed to add URL to visited list, cannot proceed: %s\n", page_url);
        return;
    }
    printf("Processing page for resources: %s\n", page_url);

    CURLcode res;
    struct MemoryStruct page_content;
    init_memory_struct(&page_content);

    curl_easy_setopt(curl, CURLOPT_URL, page_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&page_content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "DEBUG: curl_easy_perform() failed while fetching page %s: %s\n", page_url, curl_easy_strerror(res));
        fprintf(stderr, "DEBUG: Curl error details: %s\n", errbuf);
        free(page_content.memory);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL);
        return;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        fprintf(stderr, "DEBUG: HTTP error %ld while fetching page %s\n", http_code, page_url);
        free(page_content.memory);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL);
        return;
    }
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL);

    const char *html_ptr = page_content.memory;
    const char *base_url = "https://welearn.iiserkol.ac.in";

    while (html_ptr != NULL && *html_ptr != '\0') {
        const char *link_start = strstr(html_ptr, "<a ");
        if (!link_start) break;

        const char *href_start = strstr(link_start, "href=\"");
        if (!href_start) {
            html_ptr = link_start + 3;
            continue;
        }
        href_start += strlen("href=\"");
        const char *href_end = strchr(href_start, '"');
        if (!href_end) {
            html_ptr = href_start;
            continue;
        }

        size_t url_len = href_end - href_start;
        char current_url[MAX_URL_LEN];
        if (url_len < sizeof(current_url) && url_len > 0) {
            strncpy(current_url, href_start, url_len);
            current_url[url_len] = '\0';

            char suggested_name[MAX_FILENAME_LEN] = "";
            const char* tag_end = strchr(href_end, '>');
            if (tag_end) {
                const char* text_start = tag_end + 1;
                const char* text_end = strstr(text_start, "</a>");
                if (text_end && text_start < text_end) {
                    size_t text_len = text_end - text_start;
                    while (text_len > 0 && isspace((unsigned char)*text_start)) {
                        text_start++;
                        text_len--;
                    }
                    while (text_len > 0 && isspace((unsigned char)text_start[text_len - 1])) {
                        text_len--;
                    }

                    const char* inner_tag_start = strchr(text_start, '<');
                    if (inner_tag_start != NULL && inner_tag_start < text_start + text_len) {
                        const char* inner_tag_end = strchr(inner_tag_start, '>');
                        if(inner_tag_end && inner_tag_end < text_start + text_len) {
                            if (inner_tag_end + 1 < text_end) {
                                text_start = inner_tag_end + 1;
                                text_len = text_end - text_start;
                                while (text_len > 0 && isspace((unsigned char)*text_start)) {
                                    text_start++;
                                    text_len--;
                                }
                                while (text_len > 0 && isspace((unsigned char)text_start[text_len - 1])) {
                                    text_len--;
                                }
                            } else {
                                text_len = 0;
                            }
                        } else {
                            text_len = 0;
                        }
                    }

                    if (text_len > 0 && text_len < sizeof(suggested_name)) {
                        strncpy(suggested_name, text_start, text_len);
                        suggested_name[text_len] = '\0';
                    }
                }
            }

            if ((strstr(current_url, "/mod/resource/view.php?id=") || strstr(current_url, "/pluginfile.php/")) && current_url[0] != '#') {
                char full_url[MAX_URL_LEN];
                if (strncmp(current_url, "http", 4) != 0) {
                    snprintf(full_url, sizeof(full_url), "%s%s", base_url, current_url);
                } else {
                    strncpy(full_url, current_url, sizeof(full_url)-1);
                    full_url[sizeof(full_url)-1] = '\0';
                }
                download_file(curl, full_url, course_path, suggested_name);
                SLEEP(1);
            }
            else if (strstr(current_url, "/mod/folder/view.php?id=") && current_url[0] != '#') {
                char full_url[MAX_URL_LEN];
                if (strncmp(current_url, "http", 4) != 0) {
                    snprintf(full_url, sizeof(full_url), "%s%s", base_url, current_url);
                } else {
                    strncpy(full_url, current_url, sizeof(full_url)-1);
                    full_url[sizeof(full_url)-1] = '\0';
                }
                printf("--- Entering Folder: %s ---\n", full_url);
                process_page_for_resources(curl, full_url, course_path, visited);
                printf("--- Exiting Folder: %s ---\n", full_url);
                SLEEP(1);
            }
        }
        html_ptr = href_end + 1;
    }

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

    const char *course_prefix = "Course: ";
    if (strncmp(start, course_prefix, strlen(course_prefix)) == 0) {
        start += strlen(course_prefix);
    }
    
    const char *site_name_separator = " : ";
    const char *site_sep_pos = strstr(start, site_name_separator);
    if (site_sep_pos != NULL && site_sep_pos < end) {
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

    char *trimmed_start = title;
    while (isspace((unsigned char)*trimmed_start)) {
        trimmed_start++;
    }
    if (*trimmed_start == 0) {
        free(title);
        fprintf(stderr, "DEBUG: Extracted title was all whitespace after trimming.\n");
        return NULL;
    }

    char *trimmed_end = trimmed_start + strlen(trimmed_start) - 1;
    while (trimmed_end > trimmed_start && isspace((unsigned char)*trimmed_end)) {
        trimmed_end--;
    }
    *(trimmed_end + 1) = '\0';

    char sanitized_title[MAX_PATH_LEN];
    sanitize_filename(trimmed_start, sanitized_title, sizeof(sanitized_title));

    free(title);

    if (strlen(sanitized_title) == 0) {
        fprintf(stderr, "DEBUG: Sanitized title is empty.\n");
        return NULL;
    }
    return strdup(sanitized_title);
}

// Extract course links from the main dashboard page and process each one
void extract_course_links_and_process(CURL *curl_handle, const char *html) {
    if (!html || !curl_handle) return;

    printf("\n--- Extracting and Processing Course Links ---\n");

    struct VisitedUrls visited_list;
    init_visited_urls(&visited_list);

    const char *mycourses_marker = "data-key=\"mycourses\"";
    const char *search_start_ptr = strstr(html, mycourses_marker);
    const char *html_ptr = NULL;

    if (!search_start_ptr) {
        fprintf(stderr, "DEBUG: Could not find the 'My courses' marker ('%s') in the dashboard HTML. Searching from beginning.\n", mycourses_marker);
        html_ptr = html;
    } else {
        printf("DEBUG: Found 'My courses' marker. Starting search for course links from this point.\n");
        html_ptr = search_start_ptr + strlen(mycourses_marker);
    }

    const char *specific_link_tag_start = "<a class=\"list-group-item list-group-item-action \" href=\"";
    const char *course_url_pattern = "/course/view.php?id=";
    const char *base_url = "https://welearn.iiserkol.ac.in";
    int found_courses = 0;

    while (html_ptr != NULL && *html_ptr != '\0') {
        const char *link_tag_start = strstr(html_ptr, specific_link_tag_start);
        if (!link_tag_start) break;

        const char *link_start = link_tag_start + strlen(specific_link_tag_start);

        const char *link_end = strchr(link_start, '"');
        if (!link_end) {
            html_ptr = link_start;
            continue;
        }

        size_t url_len = link_end - link_start;
        char current_url[MAX_URL_LEN];
        if (url_len < sizeof(current_url) && url_len > 0) {
            strncpy(current_url, link_start, url_len);
            current_url[url_len] = '\0';

            if (strstr(current_url, course_url_pattern)) {
                found_courses++;

                char full_course_url[MAX_URL_LEN];
                if (strncmp(current_url, "http", 4) != 0) {
                    printf("DEBUG: Warning - Course link seems relative: %s. Prepending base URL.\n", current_url);
                    snprintf(full_course_url, sizeof(full_course_url), "%s%s", base_url, current_url);
                } else {
                    strncpy(full_course_url, current_url, sizeof(full_course_url) - 1);
                    full_course_url[sizeof(full_course_url) - 1] = '\0';
                }

                printf("\nFound Course Link: %s\n", full_course_url);

                CURLcode res;
                struct MemoryStruct course_page_content;
                init_memory_struct(&course_page_content);

                curl_easy_setopt(curl_handle, CURLOPT_URL, full_course_url);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&course_page_content);
                curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
                curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 0L);
                
                char errbuf_course[CURL_ERROR_SIZE] = {0};
                curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf_course);

                res = curl_easy_perform(curl_handle);

                if (res == CURLE_OK) {
                    long http_code = 0;
                    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
                    if (http_code < 400) {
                        char *course_title = extract_course_title(course_page_content.memory);
                        if (course_title && strlen(course_title) > 0) {
                            printf("Processing Course: %s\n", course_title);

                            char course_path[MAX_PATH_LEN];
                            snprintf(course_path, sizeof(course_path), "./%s", course_title);
                            if (create_directory(course_path)) {
                                process_page_for_resources(curl_handle, full_course_url, course_path, &visited_list);
                            } else {
                                fprintf(stderr, "DEBUG: Failed to create directory for course: %s (Path: %s)\n", course_title, course_path);
                            }
                            free(course_title);
                        } else {
                            fprintf(stderr, "DEBUG: Could not extract a valid title for course: %s\n", full_course_url);
                            const char* id_param = "?id=";
                            const char* id_start = strstr(full_course_url, id_param);
                            char default_dir_name[64] = "course_unknown";
                            if (id_start){
                                id_start += strlen(id_param);
                                char id_str[16];
                                int i = 0;
                                while(isdigit((unsigned char)id_start[i]) && i < sizeof(id_str) - 1) {
                                    id_str[i] = id_start[i];
                                    i++;
                                }
                                id_str[i] = '\0';
                                if (i > 0) {
                                    snprintf(default_dir_name, sizeof(default_dir_name), "course_%s", id_str);
                                }
                            }
                            char sanitized_default_name[MAX_PATH_LEN];
                            sanitize_filename(default_dir_name, sanitized_default_name, sizeof(sanitized_default_name));
                            printf("DEBUG: Using default directory name: %s\n", sanitized_default_name);

                            char course_path[MAX_PATH_LEN];
                            snprintf(course_path, sizeof(course_path), "./%s", sanitized_default_name);
                            if (create_directory(course_path)) {
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
                curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, NULL);

                free(course_page_content.memory);
                SLEEP(2);
            }
        }

        html_ptr = link_end + 1;
    }

    if (found_courses == 0) {
        printf("DEBUG: No course links matching the specific pattern ('%s' containing '%s') were found after the 'My courses' marker.\n", specific_link_tag_start, course_url_pattern);
        if (search_start_ptr == html) {
            printf("DEBUG: Also searched from the beginning of the page.\n");
        }
    } else {
        printf("DEBUG: Found and initiated processing for %d course links.\n", found_courses);
    }

    printf("\n--- Finished Processing Course Links ---\n");

    free_visited_urls(&visited_list);
}
