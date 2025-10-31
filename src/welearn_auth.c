#include "../include/welearn_auth.h"
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#endif

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
    int i = 0;
    char ch;
    while (i < size - 1) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') {
            break;
        } else if (ch == '\b') {
            if (i > 0) {
                i--;
                printf("\b \b");
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
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (fgets(password, size, stdin) == NULL) {
        password[0] = '\0';
    } else {
        password[strcspn(password, "\n")] = 0;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
#endif
}

// Save encrypted credentials to file
int save_credentials(const char *username, const char *password, char key) {
    FILE *fp = fopen(CRED_FILE, "wb");
    if (!fp) {
        perror("DEBUG: Error opening credentials file for writing");
        return 0;
    }

    char *enc_user = strdup(username);
    char *enc_pass = strdup(password);
    if (!enc_user || !enc_pass) {
        perror("DEBUG: strdup failed in save_credentials");
        free(enc_user);
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
    // Credentials saved - silent operation for cleaner output
    return 1;
}

// Load and decrypt credentials from file
int load_credentials(char *username, size_t user_size, char *password, size_t pass_size, char key) {
    FILE *fp = fopen(CRED_FILE, "rb");
    if (!fp) {
        return 0;
    }

    char user_buf[256];
    char pass_buf[256];

    if (fgets(user_buf, sizeof(user_buf), fp) == NULL ||
        fgets(pass_buf, sizeof(pass_buf), fp) == NULL) {
        fprintf(stderr, "DEBUG: Error reading from credentials file or file is corrupt.\n");
        fclose(fp);
        return 0;
    }
    fclose(fp);

    user_buf[strcspn(user_buf, "\r\n")] = 0;
    pass_buf[strcspn(pass_buf, "\r\n")] = 0;

    encrypt_decrypt(user_buf, key);
    encrypt_decrypt(pass_buf, key);

    strncpy(username, user_buf, user_size - 1);
    username[user_size - 1] = '\0';

    strncpy(password, pass_buf, pass_size - 1);
    password[pass_size - 1] = '\0';

    if (strlen(user_buf) >= user_size || strlen(pass_buf) >= pass_size) {
        fprintf(stderr, "DEBUG: Warning: Loaded credentials might be truncated.\n");
    }

    // Credentials loaded - silent operation for cleaner output
    return 1;
}

// Extract login token from HTML form
char *extract_logintoken(const char *html) {
    if (!html) {
        return NULL;
    }
    const char *pattern = "name=\"logintoken\" value=\"";
    const char *start = strstr(html, pattern);
    if (!start) {
        // Token not found - silent return for cleaner output
        return NULL;
    }
    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end) {
        // Closing quote not found - silent return
        return NULL;
    }
    size_t len = end - start;
    char *token = malloc(len + 1);
    if (!token) {
        fprintf(stderr, "Error: Memory allocation failed for logintoken\n");
        return NULL;
    }
    strncpy(token, start, len);
    token[len] = '\0';
    return token;
}
