/* Initialization of S/390 specific backend library.
   Copyright (C) 2005, 2006 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define BACKEND		s390_
#define RELOC_PREFIX	R_390_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on arm_reloc.def.  */
#include "common-reloc.c"


const char *
s390_init (elf, machine, eh, ehlen)
     Elf *elf __attribute__ ((unused));
     GElf_Half machine __attribute__ ((unused));
     Ebl *eh;
     size_t ehlen;
{
  /* Check whether the Elf_BH object has a sufficent size.  */
  if (ehlen < sizeof (Ebl))
    return NULL;

  /* We handle it.  */
  eh->name = "IBM S/390";
  s390_init_reloc (eh);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, register_info);
  HOOK (eh, return_value_location);

  /* Only the 64-bit format uses the incorrect hash table entry size.  */
  if (eh->class == ELFCLASS64)
    eh->sysvhash_entrysize = sizeof (Elf64_Xword);

  return MODVERSION;
}
