/* PPC64 specific symbolic name handling.
   Copyright (C) 2004, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND		ppc64_
#include "libebl_CPU.h"


/* Check for the simple reloc types.  */
Elf_Type
ppc64_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type)
{
  switch (type)
    {
    case R_PPC64_ADDR64:
    case R_PPC64_UADDR64:
      return ELF_T_XWORD;
    case R_PPC64_ADDR32:
    case R_PPC64_UADDR32:
      return ELF_T_WORD;
    case R_PPC64_UADDR16:
      return ELF_T_HALF;
    default:
      return ELF_T_NUM;
    }
}


const char *
ppc64_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_PPC64_GLINK:
      return "PPC64_GLINK";
    case DT_PPC64_OPD:
      return "PPC64_OPD";
    case DT_PPC64_OPDSZ:
      return "PPC64_OPDSZ";
    default:
      break;
    }
  return NULL;
}


bool
ppc64_dynamic_tag_check (int64_t tag)
{
  return (tag == DT_PPC64_GLINK
	  || tag == DT_PPC64_OPD
	  || tag == DT_PPC64_OPDSZ);
}


/* Check whether given symbol's st_value and st_size are OK despite failing
   normal checks.  */
bool
ppc64_check_special_symbol (Elf *elf, GElf_Ehdr *ehdr,
			    const GElf_Sym *sym __attribute__ ((unused)),
			    const char *name __attribute__ ((unused)),
			    const GElf_Shdr *destshdr)
{
  const char *sname = elf_strptr (elf, ehdr->e_shstrndx, destshdr->sh_name);
  if (sname == NULL)
    return false;
  return strcmp (sname, ".opd") == 0;
}


/* Check if backend uses a bss PLT in this file.  */
bool
ppc64_bss_plt_p (Elf *elf __attribute__ ((unused)),
		 GElf_Ehdr *ehdr __attribute__ ((unused)))
{
  return true;
}
