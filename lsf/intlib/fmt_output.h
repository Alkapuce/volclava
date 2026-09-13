/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * Reusable custom output formatting helpers.
 */

#ifndef FMT_OUTPUT_H
#define FMT_OUTPUT_H

#include <stddef.h>
#include <stdio.h>

struct fmt_field_def {
    const char *name;
    const char *alias;
    const char *header;
    int default_width;
};

struct fmt_column {
    const struct fmt_field_def *field;
    int width;
    int right_align;
    int has_width;
};

struct fmt_request {
    struct fmt_column *columns;
    int num_columns;
    char delimiter;
    int has_delimiter;
};

int fmt_output_parse(const char *format, const struct fmt_field_def *fields,
                     int num_fields, struct fmt_request *request,
                     char *errbuf, size_t errbuf_len);
void fmt_output_free(struct fmt_request *request);
int fmt_output_fields_string(const struct fmt_request *request,
                             char *buf, size_t buflen);

void fmt_output_print_header(FILE *out, const struct fmt_request *request);
void fmt_output_print_value(FILE *out, const struct fmt_request *request,
                            int column_index, const char *value);
void fmt_output_print_eol(FILE *out);

#endif
