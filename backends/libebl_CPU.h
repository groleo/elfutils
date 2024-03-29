/* Common interface for libebl modules.
   Copyright (C) 2000, 2001, 2002, 2003, 2005 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _LIBEBL_CPU_H
#define _LIBEBL_CPU_H 1

#include <libeblP.h>

#define EBLHOOK(name)	EBLHOOK_1(BACKEND, name)
#define EBLHOOK_1(a, b)	EBLHOOK_2(a, b)
#define EBLHOOK_2(a, b)	a##b

/* Constructor.  */
extern const char *EBLHOOK(init) (Elf *elf, GElf_Half machine,
				  Ebl *eh, size_t ehlen);

#include "ebl-hooks.h"

#define HOOK(eh, name)	eh->name = EBLHOOK(name)

extern bool (*generic_debugscn_p) (const char *) attribute_hidden;


#endif	/* libebl_CPU.h */
