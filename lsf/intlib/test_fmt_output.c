/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * Tests for reusable custom output formatting helpers.
 */

#include "fmt_output.h"

#include <stdio.h>
#include <string.h>

static const struct fmt_field_def fields[] = {
    {"JOBID", NULL, "JOBID", 7},
    {"STATUS", "STAT", "STATUS", 6},
    {"EXEC_HOST", "HOST", "EXEC_HOST", 12}
};

static int
fail(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int
expect_success(void)
{
    struct fmt_request request;
    char errbuf[256];
    int rc = 0;

    if (fmt_output_parse("jobid:10 stat:-8 host delimiter='|'",
                         fields, 3, &request, errbuf, sizeof(errbuf)) < 0)
        return fail(errbuf);

    if (request.num_columns != 3) {
        rc = fail("unexpected column count");
        goto cleanup;
    }
    if (!request.has_delimiter || request.delimiter != '|') {
        rc = fail("delimiter was not parsed");
        goto cleanup;
    }
    if (strcmp(request.columns[0].field->name, "JOBID") != 0 ||
        request.columns[0].width != 10 ||
        request.columns[0].left_align ||
        !request.columns[0].explicit_width) {
        rc = fail("JOBID column was not parsed");
        goto cleanup;
    }
    if (strcmp(request.columns[1].field->name, "STATUS") != 0 ||
        request.columns[1].width != 8 ||
        !request.columns[1].left_align ||
        !request.columns[1].explicit_width) {
        rc = fail("STATUS alias or width was not parsed");
        goto cleanup;
    }
    if (strcmp(request.columns[2].field->name, "EXEC_HOST") != 0 ||
        request.columns[2].width != 12 ||
        request.columns[2].left_align ||
        request.columns[2].explicit_width) {
        rc = fail("EXEC_HOST alias or default width was not parsed");
        goto cleanup;
    }

cleanup:
    fmt_output_free(&request);
    return rc;
}

static int
expect_rendering(void)
{
    struct fmt_request request;
    char errbuf[256];
    char output[256];
    FILE *out;
    size_t nread;
    int rc = 0;
    const char *expected =
        "     JOBID|STATUS  |EXEC_HOST\n"
        "       123|RUN     |hostA\n";

    if (fmt_output_parse("jobid:10 stat:-8 host delimiter='|'",
                         fields, 3, &request, errbuf, sizeof(errbuf)) < 0)
        return fail(errbuf);

    out = tmpfile();
    if (!out) {
        fmt_output_free(&request);
        return fail("tmpfile failed");
    }

    fmt_output_print_header(out, &request);
    fmt_output_print_value(out, &request, 0, "123");
    fmt_output_print_value(out, &request, 1, "RUN");
    fmt_output_print_value(out, &request, 2, "hostA");
    fmt_output_print_eol(out);

    rewind(out);
    nread = fread(output, 1, sizeof(output) - 1, out);
    output[nread] = '\0';
    if (strcmp(output, expected) != 0) {
        fprintf(stderr, "unexpected rendering:\n%s", output);
        rc = 1;
    }

    fclose(out);
    fmt_output_free(&request);
    return rc;
}

static int
expect_delimiter_without_default_padding(void)
{
    struct fmt_request request;
    char errbuf[256];
    char output[256];
    FILE *out;
    size_t nread;
    int rc = 0;
    const char *expected =
        "JOBID/EXEC_HOST\n"
        "1/hostA\n";

    if (fmt_output_parse("jobid host delimiter='/'",
                         fields, 3, &request, errbuf, sizeof(errbuf)) < 0)
        return fail(errbuf);

    out = tmpfile();
    if (!out) {
        fmt_output_free(&request);
        return fail("tmpfile failed");
    }

    fmt_output_print_header(out, &request);
    fmt_output_print_value(out, &request, 0, "1");
    fmt_output_print_value(out, &request, 1, "hostA");
    fmt_output_print_eol(out);

    rewind(out);
    nread = fread(output, 1, sizeof(output) - 1, out);
    output[nread] = '\0';
    if (strcmp(output, expected) != 0) {
        fprintf(stderr, "unexpected delimiter rendering:\n%s", output);
        rc = 1;
    }

    fclose(out);
    fmt_output_free(&request);
    return rc;
}

static int
expect_space_delimiter(void)
{
    struct fmt_request request;
    char errbuf[256];

    if (fmt_output_parse("jobid delimiter=' ' stat",
                         fields, 3, &request, errbuf, sizeof(errbuf)) < 0)
        return fail(errbuf);

    if (!request.has_delimiter || request.delimiter != ' ') {
        fmt_output_free(&request);
        return fail("space delimiter was not parsed");
    }

    fmt_output_free(&request);
    return 0;
}

static int
expect_fields_string(void)
{
    struct fmt_request request;
    char errbuf[256];
    char fields_buf[32];
    int needed;

    if (fmt_output_parse("jobid stat host",
                         fields, 3, &request, errbuf, sizeof(errbuf)) < 0)
        return fail(errbuf);

    needed = fmt_output_fields_string(&request, fields_buf,
                                      sizeof(fields_buf));
    if (needed != 23 || strcmp(fields_buf, "JOBID STATUS EXEC_HOST") != 0) {
        fmt_output_free(&request);
        return fail("field string was not exported");
    }

    needed = fmt_output_fields_string(&request, fields_buf, 8);
    if (needed != 23 || strcmp(fields_buf, "JOBID S") != 0) {
        fmt_output_free(&request);
        return fail("field string truncation was not reported");
    }

    fmt_output_free(&request);
    return 0;
}

static int
expect_failure(const char *format, const char *needle)
{
    struct fmt_request request;
    char errbuf[256];

    if (fmt_output_parse(format, fields, 3, &request, errbuf,
                         sizeof(errbuf)) == 0) {
        fmt_output_free(&request);
        return fail("format unexpectedly succeeded");
    }

    if (strstr(errbuf, needle) == NULL) {
        fprintf(stderr, "unexpected error: %s\n", errbuf);
        return 1;
    }

    return 0;
}

int
main(void)
{
    if (expect_success() != 0)
        return 1;
    if (expect_rendering() != 0)
        return 1;
    if (expect_delimiter_without_default_padding() != 0)
        return 1;
    if (expect_space_delimiter() != 0)
        return 1;
    if (expect_fields_string() != 0)
        return 1;
    if (expect_failure("missing", "not a valid field") != 0)
        return 1;
    if (expect_failure("jobid:abc", "Invalid output width") != 0)
        return 1;
    if (expect_failure("jobid delimiter='|' delimiter=','",
                       "Duplicate delimiter") != 0)
        return 1;
    if (expect_failure("delimiter='||'", "Invalid delimiter") != 0)
        return 1;
    if (expect_failure("delimiter='  '", "Invalid delimiter") != 0)
        return 1;

    return 0;
}
