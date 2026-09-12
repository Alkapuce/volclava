/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * Custom output field selection state for LIM query requests.
 */

#include "lib.h"
#include "lproto.h"

static __thread char *customOutputFields;

int
ls_set_custom_output_fields(const char *fields)
{
    FREEUP(customOutputFields);

    if (fields && fields[0] != '\0') {
        customOutputFields = putstr_(fields);
        if (!customOutputFields) {
            lserrno = LSE_MALLOC;
            return -1;
        }
    }

    return 0;
}

char *
ls_get_custom_output_fields_(void)
{
    return customOutputFields ? customOutputFields : "";
}
