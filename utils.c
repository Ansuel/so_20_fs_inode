#include "utils.h"

int handle_error(char * text, int code) {
    perror(text);
    return code;
}

void* handle_error_ptr(char * text, void* code) {
    perror(text);
    return code;
}