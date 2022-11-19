/*
 * Copyright (C) 2022  Ali Abdallah <ali.abdallah@suse.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DAVFS_UTIL_H
#define __DAVFS_UTIL_H

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>

#ifdef ERR
#undef ERR
#endif 

#define ERR(fmt, ...) { \
    if (errno != 0) \
        errx(EXIT_FAILURE, fmt, ## __VA_ARGS__); \
    else { \
        fprintf(stderr, fmt, ## __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        exit(EXIT_FAILURE); \
    } \
}

#ifdef WARN
#undef WARN
#endif

#define WARN(fmt, ...) { \
    if (errno != 0) \
        warnx(fmt, ## __VA_ARGS__); \
    else { \
        fprintf(stdout, fmt, ## __VA_ARGS__); \
        fprintf(stdout, "\n"); \
   }\
}

#ifdef __FreeBSD__
#define ERR_AT_LINE(filename, lineno, fmt, ...) {\
    fprintf(stderr, "%s:%d :", filename, lineno); \
    if (errno != 0) \
        err(EXIT_FAILURE, fmt, ## __VA_ARGS__); \
    else \
        fprintf(stderr, fmt, ## __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(EXIT_FAILURE); \
}
#endif

extern void ERR_AT_LINE(const char* filename, int lineno, const char *fmt, ...);
#ifdef __linux__
//#include <err.h>
//#define ERR_AT_LINE(filename, lineno, fmt, ...) \
#endif

#ifdef __FreeBSD__
#define mcanonicalize_file_name(path) \
    realpath(path, NULL)
#endif

#ifdef __linux__
#define mcanonicalize_file_name(path) \
    realpath(path, NULL)
//#define mcanonicalize_file_name canonicalize_file_name 
#endif

#endif /* __DAVFS_UTIL_H */
