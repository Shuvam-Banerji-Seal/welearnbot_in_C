#ifndef WELEARN_AUTH_H
#define WELEARN_AUTH_H

#include "welearn_common.h"

// Authentication functions
void encrypt_decrypt(char *text, char key);
void get_password(char *password, size_t size);
int save_credentials(const char *username, const char *password, char key);
int load_credentials(char *username, size_t user_size, char *password, size_t pass_size, char key);
char *extract_logintoken(const char *html);

#endif // WELEARN_AUTH_H
