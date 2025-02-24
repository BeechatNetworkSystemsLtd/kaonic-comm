/*
 * Copyright (c) 2025 Beechat Network Systems Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/*****************************************************************************/

#ifndef KAONIC_COMM_RFNET_PORT_H__
#define KAONIC_COMM_RFNET_PORT_H__

/*****************************************************************************/

#include <stdio.h>

/*****************************************************************************/

#define rfnet_log(...)       \
    do {                     \
        printf("rfnet:> ");  \
        printf(__VA_ARGS__); \
        printf("\n\r");      \
    } while (0)

/*****************************************************************************/

#endif // KAONIC_COMM_RFNET_PORT_H__
