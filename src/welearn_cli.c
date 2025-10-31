#include "../include/welearn_common.h"
#include "../include/welearn_auth.h"
#include "../include/welearn_download.h"

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

    extract_course_links_and_process(curl, login_page_content.memory);

    free(login_page_content.memory);

cleanup:
    if (logintoken) free(logintoken);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    printf("\nProgram finished.\n");
    return EXIT_SUCCESS;
}
