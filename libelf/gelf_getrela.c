/* Get RELA relocation information at given index.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Rela *
gelf_getrela (data, ndx, dst)
     Elf_Data *data;
     int ndx;
     GElf_Rela *dst;
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  Elf_Scn *scn;
  GElf_Rela *result;

  if (data_scn == NULL)
    return NULL;

  if (unlikely (ndx < 0))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      return NULL;
    }

  if (unlikely (data_scn->d.d_type != ELF_T_RELA))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* This is the one place where we have to take advantage of the fact
     that an `Elf_Data' pointer is also a pointer to `Elf_Data_Scn'.
     The interface is broken so that it requires this hack.  */
  scn = data_scn->s;

  rwlock_rdlock (scn->elf->lock);

  if (scn->elf->class == ELFCLASS32)
    {
      /* We have to convert the data.  */
      if (unlikely ((ndx + 1) * sizeof (Elf32_Rela) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  result = NULL;
	}
      else
	{
	  Elf32_Rela *src = &((Elf32_Rela *) data_scn->d.d_buf)[ndx];

	  dst->r_offset = src->r_offset;
	  dst->r_info = GELF_R_INFO (ELF32_R_SYM (src->r_info),
				     ELF32_R_TYPE (src->r_info));
	  dst->r_addend = src->r_addend;

	  result = dst;
	}
    }
  else
    {
      /* Simply copy the data after we made sure we are actually getting
	 correct data.  */
      if (unlikely ((ndx + 1) * sizeof (Elf64_Rela) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  result = NULL;
	}
      else
	result = memcpy (dst, &((Elf64_Rela *) data_scn->d.d_buf)[ndx],
			 sizeof (Elf64_Rela));
    }

  rwlock_unlock (scn->elf->lock);

  return result;
}
