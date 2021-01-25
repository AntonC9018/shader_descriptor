#pragma once
#include <stdio.h>

struct Writer
{
    FILE *stream;
    int current_indentation_level;
    int spaces_per_tab = 4;
};

inline void wr_indent(Writer* writer)
{
    writer->current_indentation_level++;
}

inline void wr_unindent(Writer* writer)
{
    writer->current_indentation_level--;
}

inline void wr_print_indent(Writer* writer)
{
    if (writer->current_indentation_level)
    {
        fprintf(writer->stream, "%*c", writer->current_indentation_level * writer->spaces_per_tab, ' ');
    }
}

inline void wr_puts(Writer* writer, const char* string)
{
    fputs(string, writer->stream);
}

inline void wr_line(Writer* writer, const char* string)
{
    wr_print_indent(writer);
    fprintf(writer->stream, "%s\n", string);
}

inline void wr_lines(Writer* writer, int count, const char** strings)
{
    for (int i = 0; i < count; i++)
    {
        wr_line(writer, strings[i]);
    }
}

inline void wr_putc(Writer* writer, char ch)
{
    fputc(ch, writer->stream);
}

inline void wr_start_block(Writer* writer)
{
    wr_line(writer, "{");
    wr_indent(writer);
}

inline void wr_end_block(Writer* writer)
{
    wr_unindent(writer);
    wr_line(writer, "}");
}

inline void wr_start_struct(Writer* writer)
{
    wr_start_block(writer);
}

inline void wr_end_struct(Writer* writer)
{
    wr_unindent(writer);
    wr_line(writer, "};");
}

inline void wr_flush(Writer* writer)
{
    fflush(writer->stream);
}

#define wr_format(writer, string, ...) fprintf((writer)->stream, string, __VA_ARGS__)

#define wr_format_line(writer, string, ...)      \
    wr_print_indent((writer)); wr_format((writer), string"\n", __VA_ARGS__)

