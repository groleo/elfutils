/* Update RELA relocation information at given index.
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
#include <stdlib.h>

#include "libelfP.h"


int
gelf_update_rela (Elf_Data *dst, int ndx, GElf_Rela *src)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) dst;
  Elf_Scn *scn;
  int result = 0;

  if (dst == NULL)
    return 0;

  if (unlikely (ndx < 0))
    {
      __libelf_seterrno (ELF_E_INVALID_INDEX);
      return 0;
    }

  if (unlikely (data_scn->d.d_type != ELF_T_RELA))
    {
      /* The type of the data better should match.  */
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return 0;
    }

  scn = data_scn->s;
  rwlock_wrlock (scn->elf->lock);

  if (scn->elf->class == ELFCLASS32)
    {
      Elf32_Rela *rel;

      /* There is the possibility that the values in the input are
	 too large.  */
      if (unlikely (src->r_offset > 0xffffffffull)
	  || unlikely (GELF_R_SYM (src->r_info) > 0xffffff)
	  || unlikely (GELF_R_TYPE (src->r_info) > 0xff)
	  || unlikely (src->r_addend < -0x80000000ll)
	  || unlikely (src->r_addend > 0x7fffffffll))
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      /* Check whether we have to resize the data buffer.  */
      if (unlikely ((ndx + 1) * sizeof (Elf32_Rela) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      rel = &((Elf32_Rela *) data_scn->d.d_buf)[ndx];

      rel->r_offset = src->r_offset;
      rel->r_info = ELF32_R_INFO (GELF_R_SYM (src->r_info),
				  GELF_R_TYPE (src->r_info));
      rel->r_addend = src->r_addend;
    }
  else
    {
      /* Check whether we have to resize the data buffer.  */
      if (unlikely ((ndx + 1) * sizeof (Elf64_Rela) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      ((Elf64_Rela *) data_scn->d.d_buf)[ndx] = *src;
    }

  result = 1;

  /* Mark the section as modified.  */
  scn->flags |= ELF_F_DIRTY;

 out:
  rwlock_unlock (scn->elf->lock);

  return result;
}
