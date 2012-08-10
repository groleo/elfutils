/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_FIXUP_H
#define ANDROID_FIXUP_H

#include <stdio.h>
#include <libgen.h>     // for basename

//#define _OFF_T_DEFINED_

//#define off_t           loff_t

#ifndef MAX
#define MAX(x,y)        ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x,y)        ((x) < (y) ? (x) : (y))
#endif

#ifndef powerof2
#define powerof2(x)     (((x - 1) & (x)) == 0)
#endif

/* workaround for canonicalize_file_name */
#define canonicalize_file_name(path) realpath(path, NULL)

/* workaround for open64 */
#define open64(path, flags)     open(path, ((flags) | O_LARGEFILE))

/* no internalization */
#define gettext(x)      (x)

/* _mempcpy and mempcpy */
#ifndef __mempcpy
#define __mempcpy(dest, src, n)  mempcpy(dest, src, n)
#endif

#ifndef mempcpy
#include <string.h>

static inline void *mempcpy(void *dest, const void *src, size_t n)
{
    char *ptr = memcpy(dest, src, n);
    return ptr + n;
}
#endif

/* rawmemchr */
static inline void *rawmemchr(const void *s, int c)
{
    const unsigned char *ptr = s;
    while (1) {
        if (*ptr == c) return (void *) ptr;
        ptr++;
    }
}

/* workaround for stpcpy */
static inline char *stpcpy(char *dst, const char *src)
{
    while (*src) {
        *dst++ = *src++;
    }
    return dst;
}

/* forward declarations */
//char * dgettext (const char * domainname, const char * msgid);
#define dgettext(d, m) (m)

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

#define ARMAG   "!<arch>\n"     /* String that begins an archive file.  */
#define SARMAG  8               /* Size of that string.  */
#define ARFMAG  "`\n"           /* String in ar_fmag at end of each header.  */

#if 0
struct ar_hdr
  {
    char ar_name[16];           /* Member file name, sometimes / terminated. */
    char ar_date[12];           /* File date, decimal seconds since Epoch.  */
    char ar_uid[6], ar_gid[6];  /* User and group IDs, in ASCII decimal.  */
    char ar_mode[8];            /* File mode, in ASCII octal.  */
    char ar_size[10];           /* File size, in ASCII decimal.  */
    char ar_fmag[2];            /* Always contains ARFMAG.  */
  };
#endif

#include <alloca.h>
inline __attribute__((always_inline)) char *strdupa(const char *s)
{
    char *d = alloca(strlen(s) + 1);
    if (s && d)
        strcpy(d, s);
    return d;
}

inline __attribute__((always_inline)) char *strndupa(const char *s, size_t n)
{
    char *d = alloca((strlen(s) + 1) < n ? (strlen(s) + 1) : n);
    if (s && d)
        strncpy(d, s, n);
    return d;
}

#endif /* ANDROID_FIXUP_H */
