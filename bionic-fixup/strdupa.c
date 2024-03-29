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

#ifndef ELFUTILS_STRDUPA_H
#define ELFUTILS_STRDUPA_H

#include <alloca.h>
#include <string.h>

inline char *strdupa(const char *s)
{
    char *d = alloca(strlen(s) + 1);
    if (s && d)
        strcpy(d, s);
    return d;
}

inline char *strdnupa(const char *s, size_t n)
{
    char *d = alloca((strlen(s) + 1) < n ? (strlen(s) + 1) : n);
    if (s && d)
        strncpy(d, s, n);
    return d;
}

#endif
