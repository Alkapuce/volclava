/*
 * Copyright (C) 2026 Bytedance Ltd. and/or its affiliates
 *
 * Custom output field selection state for batch query requests.
 */

#include "lsb.h"

static __thread char *customOutputFields;

int
lsb_set_custom_output_fields(const char *fields)
{
    FREEUP(customOutputFields);

    if (fields && fields[0] != '\0') {
        customOutputFields = putstr_(fields);
        if (!customOutputFields) {
            lsberrno = LSBE_NO_MEM;
            return -1;
        }
    }

    return 0;
}

char *
lsb_get_custom_output_fields_(void)
{
    return customOutputFields ? customOutputFields : "";
}
