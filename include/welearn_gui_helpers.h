#ifndef WELEARN_GUI_HELPERS_H
#define WELEARN_GUI_HELPERS_H

#include <gtk/gtk.h>
#include "welearn_common.h"

// File item structure for tree model
typedef struct FileItem {
    char *title;
    char *url;
    char *suggested_name;
    char *course_path;
    gboolean is_folder;
    gboolean is_course;
    GtkTreeIter *parent_iter;
    struct FileItem *next;
} FileItem;

// Course structure
typedef struct Course {
    char *title;
    char *url;
    FileItem *files;
    struct Course *next;
} Course;

// Function declarations for file discovery
Course* discover_courses(CURL *curl, const char *dashboard_html);
FileItem* discover_course_files(CURL *curl, const char *course_url, const char *course_path);
void free_courses(Course *courses);
void free_file_items(FileItem *items);

#endif // WELEARN_GUI_HELPERS_H
