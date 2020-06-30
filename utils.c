#include "utils.h"

int handle_error(char * text, int code) {
    perror(text);
    return code;
}