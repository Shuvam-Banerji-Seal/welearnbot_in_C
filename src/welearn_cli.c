#include "../include/welearn_common.h"
#include "../include/welearn_auth.h"
#include "../include/welearn_download.h"
#include <ctype.h>

// Helper function to get user input for download directory
void get_download_directory(char *path, size_t size) {
    printf("\nEnter download directory path (press Enter for current directory '.'): ");
    fflush(stdout);
    
    if (fgets(path, size, stdin) == NULL) {
        strcpy(path, ".");
        return;
    }
    
    // Remove trailing newline
    path[strcspn(path, "\n")] = 0;
    
    // If empty, use current directory
    if (strlen(path) == 0) {
        strcpy(path, ".");
    }
}

// Helper function to parse comma-separated file selections
int parse_selections(const char *input, int **selections, size_t max_files) {
    if (!input || !selections) return 0;
    
    // Count commas to estimate size
    int count = 1;
    for (const char *p = input; *p; p++) {
        if (*p == ',') count++;
    }
    
    *selections = malloc(count * sizeof(int));
    if (!*selections) return 0;
    
    int idx = 0;
    char buffer[32];
    int buf_idx = 0;
    
    for (const char *p = input; ; p++) {
        if (*p == ',' || *p == '\0' || *p == '\n') {
            if (buf_idx > 0) {
                buffer[buf_idx] = '\0';
                int num = atoi(buffer);
                if (num > 0 && (size_t)num <= max_files) {
                    (*selections)[idx++] = num;
                }
                buf_idx = 0;
            }
            if (*p == '\0' || *p == '\n') break;
        } else if (isdigit((unsigned char)*p) || *p == ' ') {
            if (*p != ' ' && buf_idx < (int)sizeof(buffer) - 1) {
                buffer[buf_idx++] = *p;
            }
        }
    }
    
    return idx;
}

int main(void) {
    CURL *curl;
    CURLcode res;
    char username[128];
    char password[128];
    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return EXIT_FAILURE;
    }

    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    if (!load_credentials(username, sizeof(username), password, sizeof(password), ENCRYPTION_KEY)) {
        printf("Credentials not found or failed to load.\nPlease enter your WeLearn credentials:\n");
        printf("Username: ");
        fflush(stdout);
        if (fgets(username, sizeof(username), stdin) == NULL) {
            fprintf(stderr, "Error reading username.\n");
            goto cleanup;
        }
        username[strcspn(username, "\n")] = 0;

        get_password(password, sizeof(password));

        if (strlen(username) == 0 || strlen(password) == 0) {
            fprintf(stderr, "Username or password cannot be empty.\n");
            goto cleanup;
        }

        printf("Save credentials for future use? (y/n): ");
        fflush(stdout);
        int choice_char = getchar();
        char choice = (char)choice_char;
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

    const char *login_url = "https://welearn.iiserkol.ac.in/login/index.php";
    struct MemoryStruct login_page_content;
    init_memory_struct(&login_page_content);

    curl_easy_setopt(curl, CURLOPT_URL, login_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

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
        free(login_page_content.memory);
        goto cleanup;
    }
    printf("Login token extracted successfully.\n");

    char *escaped_username = curl_easy_escape(curl, username, 0);
    char *escaped_password = curl_easy_escape(curl, password, 0);
    if (!escaped_username || !escaped_password) {
        fprintf(stderr, "Failed to URL-encode credentials\n");
        free(login_page_content.memory);
        free(logintoken);
        goto cleanup;
    }

    char post_fields[1024];
    snprintf(post_fields, sizeof(post_fields), "username=%s&password=%s&logintoken=%s",
             escaped_username, escaped_password, logintoken);

    free(login_page_content.memory);
    init_memory_struct(&login_page_content);

    curl_easy_setopt(curl, CURLOPT_URL, login_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&login_page_content);
    curl_easy_setopt(curl, CURLOPT_REFERER, login_url);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    printf("Attempting login...\n");
    errbuf[0] = '\0';
    res = curl_easy_perform(curl);

    curl_free(escaped_username);
    curl_free(escaped_password);
    free(logintoken);
    logintoken = NULL;

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed during login POST: %s\n", curl_easy_strerror(res));
        fprintf(stderr, "Curl error details: %s\n", errbuf);
        free(login_page_content.memory);
        goto cleanup;
    }

    long http_code_login_post = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code_login_post);

    if (strstr(login_page_content.memory, "Invalid login, please try again") || strstr(login_page_content.memory, "loginerrors")) {
        fprintf(stderr, "Login failed! Please check your username and password.\n");
        free(login_page_content.memory);
        goto cleanup;
    }
    if (!strstr(login_page_content.memory, "/login/logout.php")) {
        fprintf(stderr, "Warning: Login might have failed - Logout link not found on the resulting page.\n");
    }

    printf("Login successful!\n");

    // NEW INTERACTIVE MODE
    printf("\n===========================================\n");
    printf("  WeLearn File Download Manager\n");
    printf("===========================================\n");
    printf("\nChoose an option:\n");
    printf("1. Download all files (old behavior)\n");
    printf("2. Select specific files to download (new)\n");
    printf("\nEnter choice (1 or 2): ");
    fflush(stdout);
    
    char choice[10];
    if (fgets(choice, sizeof(choice), stdin) == NULL) {
        strcpy(choice, "1");  // Default to old behavior, properly null-terminated
    }
    
    if (choice[0] == '2') {
        // NEW MODE: Scan and collect files
        struct FileList file_list;
        init_file_list(&file_list);
        
        printf("\nScanning courses and collecting file information...\n");
        scan_courses_and_collect_files(curl, login_page_content.memory, &file_list);
        
        if (file_list.count == 0) {
            printf("No files found.\n");
            free_file_list(&file_list);
            free(login_page_content.memory);
            goto cleanup;
        }
        
        // Display files
        printf("\nHow would you like to view the files?\n");
        printf("1. Tree view (hierarchical)\n");
        printf("2. List view (simple table)\n");
        printf("\nEnter choice (1 or 2): ");
        fflush(stdout);
        
        char view_choice[10];
        if (fgets(view_choice, sizeof(view_choice), stdin) == NULL) {
            strcpy(view_choice, "1");  // Default properly null-terminated
        }
        
        if (view_choice[0] == '2') {
            display_file_list(&file_list);
        } else {
            display_file_tree(&file_list);
        }
        
        // Get download directory
        char download_path[MAX_PATH_LEN];
        get_download_directory(download_path, sizeof(download_path));
        
        // Create download directory if it doesn't exist
        if (!create_directory(download_path)) {
            fprintf(stderr, "Failed to create download directory: %s\n", download_path);
            free_file_list(&file_list);
            free(login_page_content.memory);
            goto cleanup;
        }
        
        // Select files to download
        printf("\nSelect files to download:\n");
        printf("  - Enter 'all' to download all files\n");
        printf("  - Enter file numbers separated by commas (e.g., 1,3,5,7)\n");
        printf("  - Enter 'q' to quit without downloading\n");
        printf("\nYour selection: ");
        fflush(stdout);
        
        char selection_input[1024];
        if (fgets(selection_input, sizeof(selection_input), stdin) == NULL) {
            printf("No selection made.\n");
            free_file_list(&file_list);
            free(login_page_content.memory);
            goto cleanup;
        }
        
        // Remove trailing newline
        selection_input[strcspn(selection_input, "\n")] = 0;
        
        if (selection_input[0] == 'q' || selection_input[0] == 'Q') {
            printf("Quitting without downloading.\n");
            free_file_list(&file_list);
            free(login_page_content.memory);
            goto cleanup;
        }
        
        int *selections = NULL;
        size_t selection_count = 0;
        
        if (strcmp(selection_input, "all") == 0 || strcmp(selection_input, "ALL") == 0) {
            // Download all files
            selection_count = file_list.count;
            selections = malloc(selection_count * sizeof(int));
            if (selections) {
                for (size_t i = 0; i < selection_count; i++) {
                    selections[i] = (int)(i + 1);
                }
            }
        } else {
            // Parse specific selections
            selection_count = parse_selections(selection_input, &selections, file_list.count);
        }
        
        if (selection_count > 0 && selections) {
            printf("\nPreparing to download %zu file(s)...\n", selection_count);
            download_selected_files(curl, &file_list, selections, selection_count, download_path);
            free(selections);
        } else {
            printf("No valid selections made.\n");
        }
        
        free_file_list(&file_list);
        
    } else {
        // OLD MODE: Download everything immediately
        printf("\nDownloading all files to current directory...\n");
        extract_course_links_and_process(curl, login_page_content.memory);
    }

    free(login_page_content.memory);

cleanup:
    if (logintoken) free(logintoken);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    printf("\nProgram finished.\n");
    return EXIT_SUCCESS;
}
