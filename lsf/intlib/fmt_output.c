/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * Reusable custom output formatting helpers.
 */

#include "fmt_output.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int
fmt_set_error(char *errbuf, size_t errbuf_len, const char *msg,
              const char *arg)
{
    if (errbuf && errbuf_len > 0) {
        if (arg)
            snprintf(errbuf, errbuf_len, msg, arg);
        else
            snprintf(errbuf, errbuf_len, "%s", msg);
    }

    return -1;
}

static int
fmt_name_equal(const char *name, const char *token, size_t token_len)
{
    size_t i;

    if (!name)
        return 0;

    for (i = 0; i < token_len; i++) {
        if (name[i] == '\0')
            return 0;
        if (toupper((unsigned char)name[i]) !=
            toupper((unsigned char)token[i]))
            return 0;
    }

    return name[token_len] == '\0';
}

static const struct fmt_field_def *
fmt_find_field(const struct fmt_field_def *fields, int num_fields,
               const char *token, size_t token_len)
{
    int i;

    for (i = 0; i < num_fields; i++) {
        if (fmt_name_equal(fields[i].name, token, token_len) ||
            fmt_name_equal(fields[i].alias, token, token_len))
            return &fields[i];
    }

    return NULL;
}

static int
fmt_parse_width(const char *text, int *width, int *left_align)
{
    long value;
    char *end = NULL;

    *left_align = 0;

    if (*text == '-') {
        *left_align = 1;
        text++;
    }

    if (*text == '\0')
        return -1;

    value = strtol(text, &end, 10);
    if (*end != '\0' || value <= 0 || value > 10000)
        return -1;

    *width = (int)value;
    return 0;
}

static int
fmt_parse_delimiter(const char *token, char *delimiter)
{
    const char prefix[] = "delimiter=";
    const char *value;
    size_t len;
    size_t i;
    size_t prefix_len = strlen(prefix);

    if (strlen(token) < prefix_len)
        return 0;

    for (i = 0; prefix[i] != '\0'; i++) {
        if (toupper((unsigned char)token[i]) !=
            toupper((unsigned char)prefix[i]))
            return 0;
    }

    value = token + strlen(prefix);
    len = strlen(value);
    if ((len == 3 && value[0] == '\'' && value[2] == '\'') ||
        (len == 3 && value[0] == '"' && value[2] == '"')) {
        *delimiter = value[1];
        return 1;
    }

    if (len == 1) {
        *delimiter = value[0];
        return 1;
    }

    return -1;
}

static const char *
fmt_token_end(const char *p)
{
    char quote = '\0';

    while (*p) {
        if (quote != '\0') {
            if (*p == quote)
                quote = '\0';
            p++;
            continue;
        }

        if (*p == '\'' || *p == '"') {
            quote = *p;
            p++;
            continue;
        }

        if (isspace((unsigned char)*p))
            break;

        p++;
    }

    return p;
}

static void
fmt_print_padded(FILE *out, const char *value, int width, int left_align)
{
    int len;

    if (!value)
        value = "-";

    len = (int)strlen(value);
    if (width <= 0 || len >= width) {
        fputs(value, out);
        return;
    }

    if (left_align)
        fprintf(out, "%-*s", width, value);
    else
        fprintf(out, "%*s", width, value);
}

int
fmt_output_parse(const char *format, const struct fmt_field_def *fields,
                 int num_fields, struct fmt_request *request,
                 char *errbuf, size_t errbuf_len)
{
    const char *p;
    int capacity = 8;

    if (!request)
        return -1;

    memset(request, 0, sizeof(*request));

    if (!format || *format == '\0')
        return fmt_set_error(errbuf, errbuf_len,
                             "Empty custom output format.", NULL);

    request->columns = calloc(capacity, sizeof(struct fmt_column));
    if (!request->columns)
        return fmt_set_error(errbuf, errbuf_len, "Out of memory.", NULL);

    p = format;
    while (*p) {
        const char *token_start;
        size_t token_len;
        char *token;
        char delimiter;
        char *width_pos;
        const char *field_start;
        size_t field_len;
        int width = 0;
        int left_align = 0;
        int explicit_width = 0;
        const struct fmt_field_def *field;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        token_start = p;
        p = fmt_token_end(p);
        token_len = (size_t)(p - token_start);

        token = malloc(token_len + 1);
        if (!token) {
            fmt_output_free(request);
            return fmt_set_error(errbuf, errbuf_len, "Out of memory.", NULL);
        }
        memcpy(token, token_start, token_len);
        token[token_len] = '\0';

        delimiter = '\0';
        switch (fmt_parse_delimiter(token, &delimiter)) {
        case 1:
            if (request->has_delimiter) {
                free(token);
                fmt_output_free(request);
                return fmt_set_error(errbuf, errbuf_len,
                                     "Duplicate delimiter option.", NULL);
            }
            request->delimiter = delimiter;
            request->has_delimiter = 1;
            free(token);
            continue;
        case -1:
        {
            int rc;

            rc = fmt_set_error(errbuf, errbuf_len,
                               "Invalid delimiter <%s>. Use one character.",
                               token);
            free(token);
            fmt_output_free(request);
            return rc;
        }
        default:
            break;
        }

        field_start = token;
        field_len = strlen(token);
        width_pos = strchr(token, ':');
        if (!width_pos)
            width_pos = strchr(token, '%');
        if (width_pos) {
            field_len = (size_t)(width_pos - token);
            explicit_width = 1;
            if (fmt_parse_width(width_pos + 1, &width, &left_align) < 0) {
                int rc;

                rc = fmt_set_error(errbuf, errbuf_len,
                                   "Invalid output width in <%s>.", token);
                free(token);
                fmt_output_free(request);
                return rc;
            }
        }

        if (field_len == 0) {
            int rc;

            rc = fmt_set_error(errbuf, errbuf_len,
                               "Invalid empty field in <%s>.", token);
            free(token);
            fmt_output_free(request);
            return rc;
        }

        field = fmt_find_field(fields, num_fields, field_start, field_len);
        if (!field) {
            int rc;

            rc = fmt_set_error(errbuf, errbuf_len,
                               "<%s> is not a valid field name.", token);
            free(token);
            fmt_output_free(request);
            return rc;
        }

        if (request->num_columns == capacity) {
            struct fmt_column *columns;

            capacity *= 2;
            columns = realloc(request->columns,
                              capacity * sizeof(struct fmt_column));
            if (!columns) {
                free(token);
                fmt_output_free(request);
                return fmt_set_error(errbuf, errbuf_len, "Out of memory.",
                                     NULL);
            }
            request->columns = columns;
        }

        request->columns[request->num_columns].field = field;
        request->columns[request->num_columns].width =
            width > 0 ? width : field->default_width;
        request->columns[request->num_columns].left_align = left_align;
        request->columns[request->num_columns].explicit_width =
            explicit_width;
        request->num_columns++;
        free(token);
    }

    if (request->num_columns == 0) {
        fmt_output_free(request);
        return fmt_set_error(errbuf, errbuf_len,
                             "No output fields were requested.", NULL);
    }

    return 0;
}

void
fmt_output_free(struct fmt_request *request)
{
    if (!request)
        return;

    free(request->columns);
    memset(request, 0, sizeof(*request));
}

int
fmt_output_fields_string(const struct fmt_request *request,
                         char *buf, size_t buflen)
{
    size_t needed = 1;
    size_t used = 0;
    int i;

    if (buf && buflen > 0)
        buf[0] = '\0';

    if (!request)
        return -1;

    for (i = 0; i < request->num_columns; i++) {
        const char *name = request->columns[i].field->name;
        size_t len = strlen(name);

        needed += len;
        if (i > 0)
            needed++;

        if (buf && buflen > 0) {
            if (i > 0 && used + 1 < buflen)
                buf[used++] = ' ';
            if (used < buflen) {
                size_t copy_len = len;

                if (copy_len > buflen - used - 1)
                    copy_len = buflen - used - 1;
                memcpy(buf + used, name, copy_len);
                used += copy_len;
                buf[used] = '\0';
            }
        }
    }

    return (int)needed;
}

void
fmt_output_print_header(FILE *out, const struct fmt_request *request)
{
    int i;

    for (i = 0; i < request->num_columns; i++) {
        fmt_output_print_value(out, request, i, request->columns[i].field->header);
    }
    fmt_output_print_eol(out);
}

void
fmt_output_print_value(FILE *out, const struct fmt_request *request,
                       int column_index, const char *value)
{
    const struct fmt_column *column;
    int width;

    if (column_index > 0) {
        if (request->has_delimiter)
            fputc(request->delimiter, out);
        else
            fputc(' ', out);
    }

    column = &request->columns[column_index];
    width = column->width;
    if (request->has_delimiter && !column->explicit_width)
        width = 0;

    fmt_print_padded(out, value, width, column->left_align);
}

void
fmt_output_print_eol(FILE *out)
{
    fputc('\n', out);
}
