/* Return build ID information for a module.
   Copyright (C) 2007-2010 Red Hat, Inc.
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

#include "libdwflP.h"

static int
found_build_id (Dwfl_Module *mod, bool set,
		const void *bits, int len, GElf_Addr vaddr)
{
  if (!set)
    /* When checking bits, we do not compare VADDR because the
       address found in a debuginfo file may not match the main
       file as modified by prelink.  */
    return 1 + (mod->build_id_len == len
		&& !memcmp (bits, mod->build_id_bits, len));

  void *copy = malloc (len);
  if (unlikely (copy == NULL))
    {
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return -1;
    }

  mod->build_id_bits = memcpy (copy, bits, len);
  mod->build_id_vaddr = vaddr;
  mod->build_id_len = len;
  return len;
}

#define NO_VADDR	((GElf_Addr) -1l)

static int
check_notes (Dwfl_Module *mod, bool set, Elf_Data *data, GElf_Addr data_vaddr)
{
  size_t pos = 0;
  GElf_Nhdr nhdr;
  size_t name_pos;
  size_t desc_pos;
  while ((pos = gelf_getnote (data, pos, &nhdr, &name_pos, &desc_pos)) > 0)
    if (nhdr.n_type == NT_GNU_BUILD_ID
	&& nhdr.n_namesz == sizeof "GNU" && !memcmp (data->d_buf + name_pos,
						     "GNU", sizeof "GNU"))
      return found_build_id (mod, set,
			     data->d_buf + desc_pos, nhdr.n_descsz,
			     data_vaddr == NO_VADDR ? 0
			     : data_vaddr + desc_pos);
  return 0;
}

int
internal_function
__libdwfl_find_build_id (Dwfl_Module *mod, bool set, Elf *elf)
{
  size_t shstrndx = SHN_UNDEF;
  int result = 0;

  Elf_Scn *scn = elf_nextscn (elf, NULL);

  if (scn == NULL)
    {
      /* No sections, have to look for phdrs.  */
      GElf_Ehdr ehdr_mem;
      GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
      size_t phnum;
      if (unlikely (ehdr == NULL)
	  || unlikely (elf_getphdrnum (elf, &phnum) != 0))
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return -1;
	}
      for (size_t i = 0; result == 0 && i < phnum; ++i)
	{
	  GElf_Phdr phdr_mem;
	  GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
	  if (likely (phdr != NULL) && phdr->p_type == PT_NOTE)
	    result = check_notes (mod, set,
				  elf_getdata_rawchunk (elf,
							phdr->p_offset,
							phdr->p_filesz,
							ELF_T_NHDR),
				  dwfl_adjusted_address (mod, phdr->p_vaddr));
	}
    }
  else
    do
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	if (likely (shdr != NULL) && shdr->sh_type == SHT_NOTE)
	  {
	    /* Determine the right sh_addr in this module.  */
	    GElf_Addr vaddr = 0;
	    if (!(shdr->sh_flags & SHF_ALLOC))
	      vaddr = NO_VADDR;
	    else if (mod->e_type != ET_REL)
	      vaddr = dwfl_adjusted_address (mod, shdr->sh_addr);
	    else if (__libdwfl_relocate_value (mod, elf, &shstrndx,
					       elf_ndxscn (scn), &vaddr))
	      vaddr = NO_VADDR;
	    result = check_notes (mod, set, elf_getdata (scn, NULL), vaddr);
	  }
      }
    while (result == 0 && (scn = elf_nextscn (elf, scn)) != NULL);

  return result;
}

int
dwfl_module_build_id (Dwfl_Module *mod,
		      const unsigned char **bits, GElf_Addr *vaddr)
{
  if (mod == NULL)
    return -1;

  if (mod->build_id_len == 0 && mod->main.elf != NULL)
    {
      /* We have the file, but have not examined it yet.  */
      int result = __libdwfl_find_build_id (mod, true, mod->main.elf);
      if (result <= 0)
	{
	  mod->build_id_len = -1;	/* Cache negative result.  */
	  return result;
	}
    }

  if (mod->build_id_len <= 0)
    return 0;

  *bits = mod->build_id_bits;
  *vaddr = mod->build_id_vaddr;
  return mod->build_id_len;
}
INTDEF (dwfl_module_build_id)
NEW_VERSION (dwfl_module_build_id, ELFUTILS_0.138)

#ifdef SHARED
COMPAT_VERSION (dwfl_module_build_id, ELFUTILS_0.130, vaddr_at_end)

int
_compat_vaddr_at_end_dwfl_module_build_id (Dwfl_Module *mod,
					   const unsigned char **bits,
					   GElf_Addr *vaddr)
{
  int result = INTUSE(dwfl_module_build_id) (mod, bits, vaddr);
  if (result > 0)
    *vaddr += (result + 3) & -4;
  return result;
}
#endif
