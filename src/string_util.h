#pragma once
#include <string.h>
#include <stdlib.h>

// Copies the given string into a fresh malloc-ed buffer
inline const char* string_copy_with_malloc(const char* src)
{
    size_t size = strlen(src) + 1;
    char* buff = (char*)malloc(size);
    memcpy(buff, src, size);
    return buff;
}

// Increments the pointer until the first occurence of a character, which isn't whitespace
inline char* trim_front(char* str, char ch = ' ')
{
    while (*str == ch)
    {
        str++;
    }
    return str;
}