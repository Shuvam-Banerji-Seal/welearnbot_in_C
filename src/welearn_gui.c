#include <gtk/gtk.h>
#include "../include/welearn_common.h"
#include "../include/welearn_auth.h"
#include "../include/welearn_download.h"
#include <pthread.h>

// Application state
typedef struct {
    GtkWidget *window;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *save_creds_check;
    GtkWidget *login_button;
    GtkWidget *status_label;
    GtkWidget *progress_bar;
    GtkWidget *log_textview;
    GtkTextBuffer *log_buffer;
    CURL *curl;
    char username[128];
    char password[128];
    int is_downloading;
    pthread_t download_thread;
} AppState;

// Progress milestones
#define PROGRESS_START 0.1
#define PROGRESS_LOGIN 0.3
#define PROGRESS_PROCESSING 0.5
#define PROGRESS_COMPLETE 1.0

// Thread data for background download
typedef struct {
    AppState *app;
    char username[128];
    char password[128];
} DownloadThreadData;

// Data structures for idle callbacks
typedef struct {
    AppState *app;
    char *message;
} StatusUpdateData;

typedef struct {
    AppState *app;
    double fraction;
    char *text;
} ProgressUpdateData;

// Forward declarations
static void append_log(AppState *app, const char *message);
static void update_status(AppState *app, const char *status);
static void update_progress(AppState *app, double fraction, const char *text);

// Append text to log view
static void append_log(AppState *app, const char *message) {
    GtkTextIter iter;
    
    // Insert the message
    gtk_text_buffer_get_end_iter(app->log_buffer, &iter);
    gtk_text_buffer_insert(app->log_buffer, &iter, message, -1);
    
    // Get a fresh iterator for the newline (previous iterator is invalidated)
    gtk_text_buffer_get_end_iter(app->log_buffer, &iter);
    gtk_text_buffer_insert(app->log_buffer, &iter, "\n", -1);
    
    // Auto-scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_get_insert(app->log_buffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(app->log_textview), mark, 0.0, FALSE, 0.0, 0.0);
}

// Update status label
static void update_status(AppState *app, const char *status) {
    gtk_label_set_text(GTK_LABEL(app->status_label), status);
}

// Update progress bar
static void update_progress(AppState *app, double fraction, const char *text) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress_bar), fraction);
    if (text) {
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->progress_bar), text);
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app->progress_bar), TRUE);
    } else {
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app->progress_bar), FALSE);
    }
}

// Idle callback wrappers for thread-safe UI updates
static gboolean idle_update_status(gpointer data) {
    StatusUpdateData *update_data = (StatusUpdateData *)data;
    update_status(update_data->app, update_data->message);
    g_free(update_data->message);
    g_free(update_data);
    return G_SOURCE_REMOVE;
}

static gboolean idle_update_progress(gpointer data) {
    ProgressUpdateData *update_data = (ProgressUpdateData *)data;
    update_progress(update_data->app, update_data->fraction, update_data->text);
    if (update_data->text) {
        g_free(update_data->text);
    }
    g_free(update_data);
    return G_SOURCE_REMOVE;
}

// Helper functions to schedule UI updates from background thread
static void schedule_status_update(AppState *app, const char *status) {
    StatusUpdateData *data = g_malloc(sizeof(StatusUpdateData));
    data->app = app;
    data->message = g_strdup(status);
    g_idle_add(idle_update_status, data);
}

static void schedule_progress_update(AppState *app, double fraction, const char *text) {
    ProgressUpdateData *data = g_malloc(sizeof(ProgressUpdateData));
    data->app = app;
    data->fraction = fraction;
    data->text = text ? g_strdup(text) : NULL;
    g_idle_add(idle_update_progress, data);
}

// Download thread function
static void* download_thread_func(void *data) {
    DownloadThreadData *thread_data = (DownloadThreadData *)data;
    AppState *app = thread_data->app;
    
    schedule_status_update(app, "Logging in...");
    schedule_progress_update(app, PROGRESS_START, "Fetching login page...");
    
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(app->curl, CURLOPT_ERRORBUFFER, errbuf);
    
    // Fetch login page
    append_log(app, "Fetching login page...");
    const char *login_url = "https://welearn.iiserkol.ac.in/login/index.php";
    struct MemoryStruct login_page_content;
    init_memory_struct(&login_page_content);
    
    curl_easy_setopt(app->curl, CURLOPT_URL, login_url);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(app->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(app->curl, CURLOPT_FAILONERROR, 0L);
    
    CURLcode res = curl_easy_perform(app->curl);
    if (res != CURLE_OK) {
        append_log(app, "Failed to fetch login page");
        free(login_page_content.memory);
        schedule_status_update(app, "Error: Failed to fetch login page");
        app->is_downloading = 0;
        free(thread_data);
        return NULL;
    }
    
    long http_code = 0;
    curl_easy_getinfo(app->curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        append_log(app, "HTTP error fetching login page");
        free(login_page_content.memory);
        schedule_status_update(app, "Error: HTTP error fetching login page");
        app->is_downloading = 0;
        free(thread_data);
        return NULL;
    }
    
    char *logintoken = extract_logintoken(login_page_content.memory);
    if (!logintoken) {
        append_log(app, "Failed to extract login token");
        free(login_page_content.memory);
        schedule_status_update(app, "Error: Failed to extract login token");
        app->is_downloading = 0;
        free(thread_data);
        return NULL;
    }
    
    append_log(app, "Login token extracted successfully");
    schedule_progress_update(app, PROGRESS_LOGIN, "Logging in...");
    
    // Perform login
    char *escaped_username = curl_easy_escape(app->curl, thread_data->username, 0);
    char *escaped_password = curl_easy_escape(app->curl, thread_data->password, 0);
    
    char post_fields[1024];
    snprintf(post_fields, sizeof(post_fields), "username=%s&password=%s&logintoken=%s",
             escaped_username, escaped_password, logintoken);
    
    free(login_page_content.memory);
    init_memory_struct(&login_page_content);
    
    curl_easy_setopt(app->curl, CURLOPT_URL, login_url);
    curl_easy_setopt(app->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(app->curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(app->curl, CURLOPT_REFERER, login_url);
    
    append_log(app, "Attempting login...");
    res = curl_easy_perform(app->curl);
    
    curl_free(escaped_username);
    curl_free(escaped_password);
    free(logintoken);
    
    if (res != CURLE_OK) {
        append_log(app, "Login request failed");
        free(login_page_content.memory);
        schedule_status_update(app, "Error: Login request failed");
        app->is_downloading = 0;
        free(thread_data);
        return NULL;
    }
    
    if (strstr(login_page_content.memory, "Invalid login, please try again") || 
        strstr(login_page_content.memory, "loginerrors")) {
        append_log(app, "Login failed! Invalid credentials");
        free(login_page_content.memory);
        schedule_status_update(app, "Error: Invalid credentials");
        app->is_downloading = 0;
        free(thread_data);
        return NULL;
    }
    
    append_log(app, "Login successful!");
    schedule_status_update(app, "Extracting courses...");
    schedule_progress_update(app, PROGRESS_PROCESSING, "Processing courses...");
    append_log(app, "Extracting and processing courses...");
    
    // Process courses
    extract_course_links_and_process(app->curl, login_page_content.memory);
    
    free(login_page_content.memory);
    
    append_log(app, "Download complete!");
    schedule_status_update(app, "Download complete!");
    schedule_progress_update(app, PROGRESS_COMPLETE, "Completed");
    
    app->is_downloading = 0;
    free(thread_data);
    return NULL;
}

// Login button clicked
static void on_login_clicked(GtkButton *button, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    
    if (app->is_downloading) {
        append_log(app, "Download already in progress!");
        return;
    }
    
    const char *username = gtk_editable_get_text(GTK_EDITABLE(app->username_entry));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(app->password_entry));
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        append_log(app, "Please enter both username and password");
        return;
    }
    
    // Save credentials if checkbox is checked
    gboolean save_creds = gtk_check_button_get_active(GTK_CHECK_BUTTON(app->save_creds_check));
    if (save_creds) {
        save_credentials(username, password, ENCRYPTION_KEY);
        append_log(app, "Credentials saved");
    }
    
    // Clear log
    gtk_text_buffer_set_text(app->log_buffer, "", -1);
    
    // Start download in background thread
    app->is_downloading = 1;
    update_status(app, "Logging in...");
    update_progress(app, PROGRESS_START, "Processing...");
    
    DownloadThreadData *thread_data = g_malloc(sizeof(DownloadThreadData));
    thread_data->app = app;
    strncpy(thread_data->username, username, sizeof(thread_data->username) - 1);
    strncpy(thread_data->password, password, sizeof(thread_data->password) - 1);
    
    pthread_create(&app->download_thread, NULL, download_thread_func, thread_data);
    pthread_detach(app->download_thread);
}

// Load saved credentials
static void load_saved_credentials(AppState *app) {
    char username[128];
    char password[128];
    
    if (load_credentials(username, sizeof(username), password, sizeof(password), ENCRYPTION_KEY)) {
        gtk_editable_set_text(GTK_EDITABLE(app->username_entry), username);
        gtk_editable_set_text(GTK_EDITABLE(app->password_entry), password);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(app->save_creds_check), TRUE);
        append_log(app, "Loaded saved credentials");
    }
}

// Application activation
static void activate(GtkApplication *gtk_app, gpointer user_data) {
    AppState *app = g_malloc0(sizeof(AppState));
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_ALL);
    app->curl = curl_easy_init();
    
    curl_easy_setopt(app->curl, CURLOPT_COOKIEJAR, "cookies.txt");
    curl_easy_setopt(app->curl, CURLOPT_COOKIEFILE, "cookies.txt");
    curl_easy_setopt(app->curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(app->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(app->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(app->curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Create window
    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "WeLearn Downloader");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 800, 600);
    
    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_window_set_child(GTK_WINDOW(app->window), main_box);
    
    // Title
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='x-large' weight='bold'>WeLearn Course Downloader</span>");
    gtk_box_append(GTK_BOX(main_box), title_label);
    
    // Credentials frame
    GtkWidget *cred_frame = gtk_frame_new("Credentials");
    gtk_box_append(GTK_BOX(main_box), cred_frame);
    
    GtkWidget *cred_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(cred_box, 10);
    gtk_widget_set_margin_end(cred_box, 10);
    gtk_widget_set_margin_top(cred_box, 10);
    gtk_widget_set_margin_bottom(cred_box, 10);
    gtk_frame_set_child(GTK_FRAME(cred_frame), cred_box);
    
    // Username
    GtkWidget *username_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_widget_set_size_request(username_label, 100, -1);
    gtk_widget_set_halign(username_label, GTK_ALIGN_START);
    app->username_entry = gtk_entry_new();
    gtk_widget_set_hexpand(app->username_entry, TRUE);
    gtk_box_append(GTK_BOX(username_box), username_label);
    gtk_box_append(GTK_BOX(username_box), app->username_entry);
    gtk_box_append(GTK_BOX(cred_box), username_box);
    
    // Password
    GtkWidget *password_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_widget_set_size_request(password_label, 100, -1);
    gtk_widget_set_halign(password_label, GTK_ALIGN_START);
    app->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(app->password_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(app->password_entry), '*');
    gtk_widget_set_hexpand(app->password_entry, TRUE);
    gtk_box_append(GTK_BOX(password_box), password_label);
    gtk_box_append(GTK_BOX(password_box), app->password_entry);
    gtk_box_append(GTK_BOX(cred_box), password_box);
    
    // Save credentials checkbox
    app->save_creds_check = gtk_check_button_new_with_label("Save credentials (basic encryption)");
    gtk_box_append(GTK_BOX(cred_box), app->save_creds_check);
    
    // Login button
    app->login_button = gtk_button_new_with_label("Download Courses");
    gtk_widget_add_css_class(app->login_button, "suggested-action");
    g_signal_connect(app->login_button, "clicked", G_CALLBACK(on_login_clicked), app);
    gtk_box_append(GTK_BOX(cred_box), app->login_button);
    
    // Status
    app->status_label = gtk_label_new("Ready");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), app->status_label);
    
    // Progress bar
    app->progress_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(main_box), app->progress_bar);
    
    // Log frame
    GtkWidget *log_frame = gtk_frame_new("Log");
    gtk_widget_set_vexpand(log_frame, TRUE);
    gtk_box_append(GTK_BOX(main_box), log_frame);
    
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_frame_set_child(GTK_FRAME(log_frame), scrolled);
    
    app->log_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->log_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->log_textview), GTK_WRAP_WORD);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app->log_textview), TRUE);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_textview));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), app->log_textview);
    
    // Load saved credentials
    load_saved_credentials(app);
    
    gtk_window_present(GTK_WINDOW(app->window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.welearn.downloader", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    curl_global_cleanup();
    return status;
}
