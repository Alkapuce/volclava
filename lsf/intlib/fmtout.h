/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef FMT_OUTPUT_H
#define FMT_OUTPUT_H

#include <stddef.h>
#include <stdio.h>
#include "cJSON.h"

#define FMT_FIELD_KEEP_TAIL 1u

struct fmt_field_def {
    const char *name;
    const char *alias;
    const char *header;
    int default_width;
    unsigned int flags;
    const char *json_key;
    unsigned long long wire_mask;
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
char *fmt_output_fields_dup(const struct fmt_request *request);
int fmt_output_field_requested(const struct fmt_request *request,
                               const char *field_name);

void fmt_output_print_header(FILE *out, const struct fmt_request *request);
void fmt_output_print_value(FILE *out, const struct fmt_request *request,
                            int column_index, const char *value);
void fmt_output_print_eol(FILE *out);

unsigned long long fmt_fields_mask(const char *, const struct fmt_field_def *, int);
int fmt_fields_requested(const char *fields, const char *name);
/* JSON values are unpadded strings; duplicate fields are emitted once. */
int fmt_json_value(cJSON *, const struct fmt_request *, int, const char *);
int fmt_json_print(FILE *, const char *, const char *, cJSON *);
#endif
