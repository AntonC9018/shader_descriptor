#pragma once
#include <stdlib.h>

struct String_Builder
{
    char* data;
    char* current;
};

inline String_Builder sb_create(size_t size)
{
    String_Builder result;
    result.data = (char*)malloc(size);
    result.current = result.data;
    return result;
}

inline void sb_free(String_Builder& sb)
{
    free(sb.data);
}

inline void sb_cat(String_Builder& sb, const char* src)
{
    while(*src != 0)
    {
        *sb.current = *src;
        sb.current++; src++;
    }
}

inline void sb_chr(String_Builder& sb, char ch)
{
    *sb.current = ch;
    sb.current++;
}

inline void sb_null_terminate(String_Builder& sb)
{
    *sb.current = 0;
}

inline void sb_reset(String_Builder& sb)
{
    sb.current = sb.data;
}

inline void sb_cat_until(String_Builder& sb, const char* src, char stop)
{
    while(*src != 0 && *src != stop)
    {
        *sb.current = *src;
        sb.current++; src++;
    }
}

inline const char* sb_build(String_Builder& sb)
{
    sb_null_terminate(sb);
    return sb.data;
}