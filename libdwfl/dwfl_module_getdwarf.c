/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2005-2011 Red Hat, Inc.
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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../libdw/libdwP.h"	/* DWARF_E_* values are here.  */


/* Open libelf FILE->fd and compute the load base of ELF as loaded in MOD.
   When we return success, FILE->elf and FILE->vaddr are set up.  */
static inline Dwfl_Error
open_elf (Dwfl_Module *mod, struct dwfl_file *file)
{
  if (file->elf == NULL)
    {
      /* CBFAIL uses errno if it's set, so clear it first in case we don't
	 set it with an open failure below.  */
      errno = 0;

      /* If there was a pre-primed file name left that the callback left
	 behind, try to open that file name.  */
      if (file->fd < 0 && file->name != NULL)
	file->fd = TEMP_FAILURE_RETRY (open64 (file->name, O_RDONLY));

      if (file->fd < 0)
	return CBFAIL;

      Dwfl_Error error = __libdw_open_file (&file->fd, &file->elf, true, false);
      if (error != DWFL_E_NOERROR)
	return error;
    }
  else if (unlikely (elf_kind (file->elf) != ELF_K_ELF))
    {
      elf_end (file->elf);
      file->elf = NULL;
      close (file->fd);
      file->fd = -1;
      return DWFL_E_BADELF;
    }

  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (file->elf, &ehdr_mem);
  if (ehdr == NULL)
    {
    elf_error:
      elf_end (file->elf);
      file->elf = NULL;
      close (file->fd);
      file->fd = -1;
      return DWFL_E (LIBELF, elf_errno ());
    }

  if (mod->e_type != ET_REL)
    {
      /* In any non-ET_REL file, we compute the "synchronization address".

	 We start with the address at the end of the first PT_LOAD
	 segment.  When prelink converts REL to RELA in an ET_DYN
	 file, it expands the space between the beginning of the
	 segment and the actual code/data addresses.  Since that
	 change wasn't made in the debug file, the distance from
	 p_vaddr to an address of interest (in an st_value or DWARF
	 data) now differs between the main and debug files.  The
	 distance from address_sync to an address of interest remains
	 consistent.

	 If there are no section headers at all (full stripping), then
	 the end of the first segment is a valid synchronization address.
	 This cannot happen in a prelinked file, since prelink itself
	 relies on section headers for prelinking and for undoing it.
	 (If you do full stripping on a prelinked file, then you get what
	 you deserve--you can neither undo the prelinking, nor expect to
	 line it up with a debug file separated before prelinking.)

	 However, when prelink processes an ET_EXEC file, it can do
	 something different.  There it juggles the "special" sections
	 (SHT_DYNSYM et al) to make space for the additional prelink
	 special sections.  Sometimes it will do this by moving a special
	 section like .dynstr after the real program sections in the first
	 PT_LOAD segment--i.e. to the end.  That changes the end address of
	 the segment, so it no longer lines up correctly and is not a valid
	 synchronization address to use.  Because of this, we need to apply
	 a different prelink-savvy means to discover the synchronization
	 address when there is a separate debug file and a prelinked main
	 file.  That is done in find_debuginfo, below.  */

      size_t phnum;
      if (unlikely (elf_getphdrnum (file->elf, &phnum) != 0))
	goto elf_error;

      file->vaddr = file->address_sync = 0;
      for (size_t i = 0; i < phnum; ++i)
	{
	  GElf_Phdr ph_mem;
	  GElf_Phdr *ph = gelf_getphdr (file->elf, i, &ph_mem);
	  if (unlikely (ph == NULL))
	    goto elf_error;
	  if (ph->p_type == PT_LOAD)
	    {
	      file->vaddr = ph->p_vaddr & -ph->p_align;
	      file->address_sync = ph->p_vaddr + ph->p_memsz;
	      break;
	    }
	}
    }

  mod->e_type = ehdr->e_type;

  /* Relocatable Linux kernels are ET_EXEC but act like ET_DYN.  */
  if (mod->e_type == ET_EXEC && file->vaddr != mod->low_addr)
    mod->e_type = ET_DYN;

  return DWFL_E_NOERROR;
}

/* We have an authoritative build ID for this module MOD, so don't use
   a file by name that doesn't match that ID.  */
static void
mod_verify_build_id (Dwfl_Module *mod)
{
  assert (mod->build_id_len > 0);

  switch (__builtin_expect (__libdwfl_find_build_id (mod, false,
						     mod->main.elf), 2))
    {
    case 2:
      /* Build ID matches as it should. */
      return;

    case -1:			/* ELF error.  */
      mod->elferr = INTUSE(dwfl_errno) ();
      break;

    case 0:			/* File has no build ID note.  */
    case 1:			/* FIle has a build ID that does not match.  */
      mod->elferr = DWFL_E_WRONG_ID_ELF;
      break;

    default:
      abort ();
    }

  /* We get here when it was the right ELF file.  Clear it out.  */
  elf_end (mod->main.elf);
  mod->main.elf = NULL;
  if (mod->main.fd >= 0)
    {
      close (mod->main.fd);
      mod->main.fd = -1;
    }
}

/* Find the main ELF file for this module and open libelf on it.
   When we return success, MOD->main.elf and MOD->main.bias are set up.  */
void
internal_function
__libdwfl_getelf (Dwfl_Module *mod)
{
  if (mod->main.elf != NULL	/* Already done.  */
      || mod->elferr != DWFL_E_NOERROR)	/* Cached failure.  */
    return;

  mod->main.fd = (*mod->dwfl->callbacks->find_elf) (MODCB_ARGS (mod),
						    &mod->main.name,
						    &mod->main.elf);
  const bool fallback = mod->main.elf == NULL && mod->main.fd < 0;
  mod->elferr = open_elf (mod, &mod->main);
  if (mod->elferr != DWFL_E_NOERROR)
    return;

  if (!mod->main.valid)
    {
      /* Clear any explicitly reported build ID, just in case it was wrong.
	 We'll fetch it from the file when asked.  */
      free (mod->build_id_bits);
      mod->build_id_bits = NULL;
      mod->build_id_len = 0;
    }
  else if (fallback)
    mod_verify_build_id (mod);

  mod->main_bias = mod->e_type == ET_REL ? 0 : mod->low_addr - mod->main.vaddr;
}

/* Search an ELF file for a ".gnu_debuglink" section.  */
static const char *
find_debuglink (Elf *elf, GElf_Word *crc)
{
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    return NULL;

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	return NULL;

      const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);
      if (name == NULL)
	return NULL;

      if (!strcmp (name, ".gnu_debuglink"))
	break;
    }

  if (scn == NULL)
    return NULL;

  /* Found the .gnu_debuglink section.  Extract its contents.  */
  Elf_Data *rawdata = elf_rawdata (scn, NULL);
  if (rawdata == NULL)
    return NULL;

  Elf_Data crcdata =
    {
      .d_type = ELF_T_WORD,
      .d_buf = crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };
  Elf_Data conv =
    {
      .d_type = ELF_T_WORD,
      .d_buf = rawdata->d_buf + rawdata->d_size - sizeof *crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return NULL;

  Elf_Data *d = gelf_xlatetom (elf, &crcdata, &conv, ehdr->e_ident[EI_DATA]);
  if (d == NULL)
    return NULL;
  assert (d == &crcdata);

  return rawdata->d_buf;
}

/* If the main file might have been prelinked, then we need to
   discover the correct synchronization address between the main and
   debug files.  Because of prelink's section juggling, we cannot rely
   on the address_sync computed from PT_LOAD segments (see open_elf).

   We will attempt to discover a synchronization address based on the
   section headers instead.  But finding a section address that is
   safe to use requires identifying which sections are SHT_PROGBITS.
   We can do that in the main file, but in the debug file all the
   allocated sections have been transformed into SHT_NOBITS so we have
   lost the means to match them up correctly.

   The only method left to us is to decode the .gnu.prelink_undo
   section in the prelinked main file.  This shows what the sections
   looked like before prelink juggled them--when they still had a
   direct correspondence to the debug file.  */
static Dwfl_Error
find_prelink_address_sync (Dwfl_Module *mod)
{
  /* The magic section is only identified by name.  */
  size_t shstrndx;
  if (elf_getshdrstrndx (mod->main.elf, &shstrndx) < 0)
    return DWFL_E_LIBELF;

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (mod->main.elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (unlikely (shdr == NULL))
	return DWFL_E_LIBELF;
      if (shdr->sh_type == SHT_PROGBITS
	  && !(shdr->sh_flags & SHF_ALLOC)
	  && shdr->sh_name != 0)
	{
	  const char *secname = elf_strptr (mod->main.elf, shstrndx,
					    shdr->sh_name);
	  if (unlikely (secname == NULL))
	    return DWFL_E_LIBELF;
	  if (!strcmp (secname, ".gnu.prelink_undo"))
	    break;
	}
    }

  if (scn == NULL)
    /* There was no .gnu.prelink_undo section.  */
    return DWFL_E_NOERROR;

  Elf_Data *undodata = elf_rawdata (scn, NULL);
  if (unlikely (undodata == NULL))
    return DWFL_E_LIBELF;

  /* Decode the section.  It consists of the original ehdr, phdrs,
     and shdrs (but omits section 0).  */

  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr;
  Elf_Data dst =
    {
      .d_buf = &ehdr,
      .d_size = sizeof ehdr,
      .d_type = ELF_T_EHDR,
      .d_version = EV_CURRENT
    };
  Elf_Data src = *undodata;
  src.d_size = gelf_fsize (mod->main.elf, ELF_T_EHDR, 1, EV_CURRENT);
  src.d_type = ELF_T_EHDR;
  if (unlikely (gelf_xlatetom (mod->main.elf, &dst, &src,
			       elf_getident (mod->main.elf, NULL)[EI_DATA])
		== NULL))
    return DWFL_E_LIBELF;

  size_t shentsize = gelf_fsize (mod->main.elf, ELF_T_SHDR, 1, EV_CURRENT);
  size_t phentsize = gelf_fsize (mod->main.elf, ELF_T_PHDR, 1, EV_CURRENT);

  uint_fast16_t phnum;
  uint_fast16_t shnum;
  if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
    {
      if (ehdr.e32.e_shentsize != shentsize
	  || ehdr.e32.e_phentsize != phentsize)
	return DWFL_E_BAD_PRELINK;
      phnum = ehdr.e32.e_phnum;
      shnum = ehdr.e32.e_shnum;
    }
  else
    {
      if (ehdr.e64.e_shentsize != shentsize
	  || ehdr.e64.e_phentsize != phentsize)
	return DWFL_E_BAD_PRELINK;
      phnum = ehdr.e64.e_phnum;
      shnum = ehdr.e64.e_shnum;
    }

  /* Since prelink does not store the zeroth section header in the undo
     section, it cannot support SHN_XINDEX encoding.  */
  if (unlikely (shnum >= SHN_LORESERVE)
      || unlikely (undodata->d_size != (src.d_size
					+ phnum * phentsize
					+ (shnum - 1) * shentsize)))
    return DWFL_E_BAD_PRELINK;

  /* We look at the allocated SHT_PROGBITS (or SHT_NOBITS) sections.  (Most
     every file will have some SHT_PROGBITS sections, but it's possible to
     have one with nothing but .bss, i.e. SHT_NOBITS.)  The special sections
     that can be moved around have different sh_type values--except for
     .interp, the section that became the PT_INTERP segment.  So we exclude
     the SHT_PROGBITS section whose address matches the PT_INTERP p_vaddr.
     For this reason, we must examine the phdrs first to find PT_INTERP.  */

  GElf_Addr main_interp = 0;
  {
    size_t main_phnum;
    if (unlikely (elf_getphdrnum (mod->main.elf, &main_phnum)))
      return DWFL_E_LIBELF;
    for (size_t i = 0; i < main_phnum; ++i)
      {
	GElf_Phdr phdr;
	if (unlikely (gelf_getphdr (mod->main.elf, i, &phdr) == NULL))
	  return DWFL_E_LIBELF;
	if (phdr.p_type == PT_INTERP)
	  {
	    main_interp = phdr.p_vaddr;
	    break;
	  }
      }
  }

  src.d_buf += src.d_size;
  src.d_type = ELF_T_PHDR;
  src.d_size = phnum * phentsize;

  GElf_Addr undo_interp = 0;
  {
    union
    {
      Elf32_Phdr p32[phnum];
      Elf64_Phdr p64[phnum];
    } phdr;
    dst.d_buf = &phdr;
    dst.d_size = sizeof phdr;
    if (unlikely (gelf_xlatetom (mod->main.elf, &dst, &src,
				 ehdr.e32.e_ident[EI_DATA]) == NULL))
      return DWFL_E_LIBELF;
    if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
      {
	for (uint_fast16_t i = 0; i < phnum; ++i)
	  if (phdr.p32[i].p_type == PT_INTERP)
	    {
	      undo_interp = phdr.p32[i].p_vaddr;
	      break;
	    }
      }
    else
      {
	for (uint_fast16_t i = 0; i < phnum; ++i)
	  if (phdr.p64[i].p_type == PT_INTERP)
	    {
	      undo_interp = phdr.p64[i].p_vaddr;
	      break;
	    }
      }
  }

  if (unlikely ((main_interp == 0) != (undo_interp == 0)))
    return DWFL_E_BAD_PRELINK;

  src.d_buf += src.d_size;
  src.d_type = ELF_T_SHDR;
  src.d_size = gelf_fsize (mod->main.elf, ELF_T_SHDR, shnum - 1, EV_CURRENT);

  union
  {
    Elf32_Shdr s32[shnum - 1];
    Elf64_Shdr s64[shnum - 1];
  } shdr;
  dst.d_buf = &shdr;
  dst.d_size = sizeof shdr;
  if (unlikely (gelf_xlatetom (mod->main.elf, &dst, &src,
			       ehdr.e32.e_ident[EI_DATA]) == NULL))
    return DWFL_E_LIBELF;

  /* Now we can look at the original section headers of the main file
     before it was prelinked.  First we'll apply our method to the main
     file sections as they are after prelinking, to calculate the
     synchronization address of the main file.  Then we'll apply that
     same method to the saved section headers, to calculate the matching
     synchronization address of the debug file.

     The method is to consider SHF_ALLOC sections that are either
     SHT_PROGBITS or SHT_NOBITS, excluding the section whose sh_addr
     matches the PT_INTERP p_vaddr.  The special sections that can be
     moved by prelink have other types, except for .interp (which
     becomes PT_INTERP).  The "real" sections cannot move as such, but
     .bss can be split into .dynbss and .bss, with the total memory
     image remaining the same but being spread across the two sections.
     So we consider the highest section end, which still matches up.  */

  GElf_Addr highest;

  inline void consider_shdr (GElf_Addr interp,
			     GElf_Word sh_type,
			     GElf_Xword sh_flags,
			     GElf_Addr sh_addr,
			     GElf_Xword sh_size)
  {
    if ((sh_flags & SHF_ALLOC)
	&& ((sh_type == SHT_PROGBITS && sh_addr != interp)
	    || sh_type == SHT_NOBITS))
      {
	const GElf_Addr sh_end = sh_addr + sh_size;
	if (sh_end > highest)
	  highest = sh_end;
      }
  }

  highest = 0;
  scn = NULL;
  while ((scn = elf_nextscn (mod->main.elf, scn)) != NULL)
    {
      GElf_Shdr sh_mem;
      GElf_Shdr *sh = gelf_getshdr (scn, &sh_mem);
      if (unlikely (sh == NULL))
	return DWFL_E_LIBELF;
      consider_shdr (main_interp, sh->sh_type, sh->sh_flags,
		     sh->sh_addr, sh->sh_size);
    }
  if (highest > mod->main.vaddr)
    {
      mod->main.address_sync = highest;

      highest = 0;
      if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
	for (size_t i = 0; i < shnum - 1; ++i)
	  consider_shdr (undo_interp, shdr.s32[i].sh_type, shdr.s32[i].sh_flags,
			 shdr.s32[i].sh_addr, shdr.s32[i].sh_size);
      else
	for (size_t i = 0; i < shnum - 1; ++i)
	  consider_shdr (undo_interp, shdr.s64[i].sh_type, shdr.s64[i].sh_flags,
			 shdr.s64[i].sh_addr, shdr.s64[i].sh_size);

      if (highest > mod->debug.vaddr)
	mod->debug.address_sync = highest;
      else
	return DWFL_E_BAD_PRELINK;
    }

  return DWFL_E_NOERROR;
}

/* Find the separate debuginfo file for this module and open libelf on it.
   When we return success, MOD->debug is set up.  */
static Dwfl_Error
find_debuginfo (Dwfl_Module *mod)
{
  if (mod->debug.elf != NULL)
    return DWFL_E_NOERROR;

  GElf_Word debuglink_crc = 0;
  const char *debuglink_file = find_debuglink (mod->main.elf, &debuglink_crc);

  mod->debug.fd = (*mod->dwfl->callbacks->find_debuginfo) (MODCB_ARGS (mod),
							   mod->main.name,
							   debuglink_file,
							   debuglink_crc,
							   &mod->debug.name);
  Dwfl_Error result = open_elf (mod, &mod->debug);
  if (result == DWFL_E_NOERROR && mod->debug.address_sync != 0)
    result = find_prelink_address_sync (mod);
  return result;
}


/* Try to find a symbol table in FILE.
   Returns DWFL_E_NOERROR if a proper one is found.
   Returns DWFL_E_NO_SYMTAB if not, but still sets results for SHT_DYNSYM.  */
static Dwfl_Error
load_symtab (struct dwfl_file *file, struct dwfl_file **symfile,
	     Elf_Scn **symscn, Elf_Scn **xndxscn,
	     size_t *syments, int *first_global, GElf_Word *strshndx)
{
  bool symtab = false;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (file->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem, *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr != NULL)
	switch (shdr->sh_type)
	  {
	  case SHT_SYMTAB:
	    symtab = true;
	    *symscn = scn;
	    *symfile = file;
	    *strshndx = shdr->sh_link;
	    *syments = shdr->sh_size / shdr->sh_entsize;
	    *first_global = shdr->sh_info;
	    if (*xndxscn != NULL)
	      return DWFL_E_NOERROR;
	    break;

	  case SHT_DYNSYM:
	    if (symtab)
	      break;
	    /* Use this if need be, but keep looking for SHT_SYMTAB.  */
	    *symscn = scn;
	    *symfile = file;
	    *strshndx = shdr->sh_link;
	    *syments = shdr->sh_size / shdr->sh_entsize;
	    break;

	  case SHT_SYMTAB_SHNDX:
	    *xndxscn = scn;
	    if (symtab)
	      return DWFL_E_NOERROR;
	    break;

	  default:
	    break;
	  }
    }

  if (symtab)
    /* We found one, though no SHT_SYMTAB_SHNDX to go with it.  */
    return DWFL_E_NOERROR;

  /* We found no SHT_SYMTAB, so any SHT_SYMTAB_SHNDX was bogus.
     We might have found an SHT_DYNSYM and set *SYMSCN et al though.  */
  *xndxscn = NULL;
  return DWFL_E_NO_SYMTAB;
}


/* Translate addresses into file offsets.
   OFFS[*] start out zero and remain zero if unresolved.  */
static void
find_offsets (Elf *elf, size_t phnum, size_t n,
	      GElf_Addr addrs[n], GElf_Off offs[n])
{
  size_t unsolved = n;
  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (phdr != NULL && phdr->p_type == PT_LOAD && phdr->p_memsz > 0)
	for (size_t j = 0; j < n; ++j)
	  if (offs[j] == 0
	      && addrs[j] >= phdr->p_vaddr
	      && addrs[j] - phdr->p_vaddr < phdr->p_filesz)
	    {
	      offs[j] = addrs[j] - phdr->p_vaddr + phdr->p_offset;
	      if (--unsolved == 0)
		break;
	    }
    }
}

/* Try to find a dynamic symbol table via phdrs.  */
static void
find_dynsym (Dwfl_Module *mod)
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (mod->main.elf, &ehdr_mem);

  size_t phnum;
  if (unlikely (elf_getphdrnum (mod->main.elf, &phnum) != 0))
    return;

  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (mod->main.elf, i, &phdr_mem);
      if (phdr == NULL)
	break;

      if (phdr->p_type == PT_DYNAMIC)
	{
	  /* Examine the dynamic section for the pointers we need.  */

	  Elf_Data *data = elf_getdata_rawchunk (mod->main.elf,
						 phdr->p_offset, phdr->p_filesz,
						 ELF_T_DYN);
	  if (data == NULL)
	    continue;

	  enum
	    {
	      i_symtab,
	      i_strtab,
	      i_hash,
	      i_gnu_hash,
	      i_max
	    };
	  GElf_Addr addrs[i_max] = { 0, };
	  GElf_Xword strsz = 0;
	  size_t n = data->d_size / gelf_fsize (mod->main.elf,
						ELF_T_DYN, 1, EV_CURRENT);
	  for (size_t j = 0; j < n; ++j)
	    {
	      GElf_Dyn dyn_mem;
	      GElf_Dyn *dyn = gelf_getdyn (data, j, &dyn_mem);
	      if (dyn != NULL)
		switch (dyn->d_tag)
		  {
		  case DT_SYMTAB:
		    addrs[i_symtab] = dyn->d_un.d_ptr;
		    continue;

		  case DT_HASH:
		    addrs[i_hash] = dyn->d_un.d_ptr;
		    continue;

		  case DT_GNU_HASH:
		    addrs[i_gnu_hash] = dyn->d_un.d_ptr;
		    continue;

		  case DT_STRTAB:
		    addrs[i_strtab] = dyn->d_un.d_ptr;
		    continue;

		  case DT_STRSZ:
		    strsz = dyn->d_un.d_val;
		    continue;

		  default:
		    continue;

		  case DT_NULL:
		    break;
		  }
	      break;
	    }

	  /* Translate pointers into file offsets.  */
	  GElf_Off offs[i_max] = { 0, };
	  find_offsets (mod->main.elf, phnum, i_max, addrs, offs);

	  /* Figure out the size of the symbol table.  */
	  if (offs[i_hash] != 0)
	    {
	      /* In the original format, .hash says the size of .dynsym.  */

	      size_t entsz = SH_ENTSIZE_HASH (ehdr);
	      data = elf_getdata_rawchunk (mod->main.elf,
					   offs[i_hash] + entsz, entsz,
					   entsz == 4 ? ELF_T_WORD
					   : ELF_T_XWORD);
	      if (data != NULL)
		mod->syments = (entsz == 4
				? *(const GElf_Word *) data->d_buf
				: *(const GElf_Xword *) data->d_buf);
	    }
	  if (offs[i_gnu_hash] != 0 && mod->syments == 0)
	    {
	      /* In the new format, we can derive it with some work.  */

	      const struct
	      {
		Elf32_Word nbuckets;
		Elf32_Word symndx;
		Elf32_Word maskwords;
		Elf32_Word shift2;
	      } *header;

	      data = elf_getdata_rawchunk (mod->main.elf, offs[i_gnu_hash],
					   sizeof *header, ELF_T_WORD);
	      if (data != NULL)
		{
		  header = data->d_buf;
		  Elf32_Word nbuckets = header->nbuckets;
		  Elf32_Word symndx = header->symndx;
		  GElf_Off buckets_at = (offs[i_gnu_hash] + sizeof *header
					 + (gelf_getclass (mod->main.elf)
					    * sizeof (Elf32_Word)
					    * header->maskwords));

		  data = elf_getdata_rawchunk (mod->main.elf, buckets_at,
					       nbuckets * sizeof (Elf32_Word),
					       ELF_T_WORD);
		  if (data != NULL && symndx < nbuckets)
		    {
		      const Elf32_Word *const buckets = data->d_buf;
		      Elf32_Word maxndx = symndx;
		      for (Elf32_Word bucket = 0; bucket < nbuckets; ++bucket)
			if (buckets[bucket] > maxndx)
			  maxndx = buckets[bucket];

		      GElf_Off hasharr_at = (buckets_at
					     + nbuckets * sizeof (Elf32_Word));
		      hasharr_at += (maxndx - symndx) * sizeof (Elf32_Word);
		      do
			{
			  data = elf_getdata_rawchunk (mod->main.elf,
						       hasharr_at,
						       sizeof (Elf32_Word),
						       ELF_T_WORD);
			  if (data != NULL
			      && (*(const Elf32_Word *) data->d_buf & 1u))
			    {
			      mod->syments = maxndx + 1;
			      break;
			    }
			  ++maxndx;
			  hasharr_at += sizeof (Elf32_Word);
			} while (data != NULL);
		    }
		}
	    }
	  if (offs[i_strtab] > offs[i_symtab] && mod->syments == 0)
	    mod->syments = ((offs[i_strtab] - offs[i_symtab])
			    / gelf_fsize (mod->main.elf,
					  ELF_T_SYM, 1, EV_CURRENT));

	  if (mod->syments > 0)
	    {
	      mod->symdata = elf_getdata_rawchunk (mod->main.elf,
						   offs[i_symtab],
						   gelf_fsize (mod->main.elf,
							       ELF_T_SYM,
							       mod->syments,
							       EV_CURRENT),
						   ELF_T_SYM);
	      if (mod->symdata != NULL)
		{
		  mod->symstrdata = elf_getdata_rawchunk (mod->main.elf,
							  offs[i_strtab],
							  strsz,
							  ELF_T_BYTE);
		  if (mod->symstrdata == NULL)
		    mod->symdata = NULL;
		}
	      if (mod->symdata == NULL)
		mod->symerr = DWFL_E (LIBELF, elf_errno ());
	      else
		{
		  mod->symfile = &mod->main;
		  mod->symerr = DWFL_E_NOERROR;
		}
	      return;
	    }
	}
    }
}

/* Try to find a symbol table in either MOD->main.elf or MOD->debug.elf.  */
static void
find_symtab (Dwfl_Module *mod)
{
  if (mod->symdata != NULL	/* Already done.  */
      || mod->symerr != DWFL_E_NOERROR) /* Cached previous failure.  */
    return;

  __libdwfl_getelf (mod);
  mod->symerr = mod->elferr;
  if (mod->symerr != DWFL_E_NOERROR)
    return;

  mod->first_global = -1; /* Unknown, unless explicitly set by load_symtab.  */

  /* First see if the main ELF file has the debugging information.  */
  Elf_Scn *symscn = NULL, *xndxscn = NULL;
  GElf_Word strshndx = 0;
  mod->symerr = load_symtab (&mod->main, &mod->symfile, &symscn,
			     &xndxscn, &mod->syments, &mod->first_global,
			     &strshndx);
  switch (mod->symerr)
    {
    default:
      return;

    case DWFL_E_NOERROR:
      break;

    case DWFL_E_NO_SYMTAB:
      /* Now we have to look for a separate debuginfo file.  */
      mod->symerr = find_debuginfo (mod);
      switch (mod->symerr)
	{
	default:
	  return;

	case DWFL_E_NOERROR:
	  mod->symerr = load_symtab (&mod->debug, &mod->symfile, &symscn,
				     &xndxscn, &mod->syments,
				     &mod->first_global, &strshndx);
	  break;

	case DWFL_E_CB:		/* The find_debuginfo hook failed.  */
	  mod->symerr = DWFL_E_NO_SYMTAB;
	  break;
	}

      switch (mod->symerr)
	{
	default:
	  return;

	case DWFL_E_NOERROR:
	  break;

	case DWFL_E_NO_SYMTAB:
	  if (symscn != NULL)
	    {
	      /* We still have the dynamic symbol table.  */
	      mod->symerr = DWFL_E_NOERROR;
	      break;
	    }

	  /* Last ditch, look for dynamic symbols without section headers.  */
	  find_dynsym (mod);
	  return;
	}
      break;
    }

  /* This does some sanity checks on the string table section.  */
  if (elf_strptr (mod->symfile->elf, strshndx, 0) == NULL)
    {
    elferr:
      mod->symerr = DWFL_E (LIBELF, elf_errno ());
      return;
    }

  /* Cache the data; MOD->syments and MOD->first_global were set above.  */

  mod->symstrdata = elf_getdata (elf_getscn (mod->symfile->elf, strshndx),
				 NULL);
  if (mod->symstrdata == NULL)
    goto elferr;

  if (xndxscn == NULL)
    mod->symxndxdata = NULL;
  else
    {
      mod->symxndxdata = elf_getdata (xndxscn, NULL);
      if (mod->symxndxdata == NULL)
	goto elferr;
    }

  mod->symdata = elf_getdata (symscn, NULL);
  if (mod->symdata == NULL)
    goto elferr;
}


/* Try to open a libebl backend for MOD.  */
Dwfl_Error
internal_function
__libdwfl_module_getebl (Dwfl_Module *mod)
{
  if (mod->ebl == NULL)
    {
      __libdwfl_getelf (mod);
      if (mod->elferr != DWFL_E_NOERROR)
	return mod->elferr;

      mod->ebl = ebl_openbackend (mod->main.elf);
      if (mod->ebl == NULL)
	return DWFL_E_LIBEBL;
    }
  return DWFL_E_NOERROR;
}

/* Try to start up libdw on DEBUGFILE.  */
static Dwfl_Error
load_dw (Dwfl_Module *mod, struct dwfl_file *debugfile)
{
  if (mod->e_type == ET_REL && !debugfile->relocated)
    {
      const Dwfl_Callbacks *const cb = mod->dwfl->callbacks;

      /* The debugging sections have to be relocated.  */
      if (cb->section_address == NULL)
	return DWFL_E_NOREL;

      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	return error;

      find_symtab (mod);
      Dwfl_Error result = mod->symerr;
      if (result == DWFL_E_NOERROR)
	result = __libdwfl_relocate (mod, debugfile->elf, true);
      if (result != DWFL_E_NOERROR)
	return result;

      /* Don't keep the file descriptors around.  */
      if (mod->main.fd != -1 && elf_cntl (mod->main.elf, ELF_C_FDREAD) == 0)
	{
	  close (mod->main.fd);
	  mod->main.fd = -1;
	}
      if (debugfile->fd != -1 && elf_cntl (debugfile->elf, ELF_C_FDREAD) == 0)
	{
	  close (debugfile->fd);
	  debugfile->fd = -1;
	}
    }

  mod->dw = INTUSE(dwarf_begin_elf) (debugfile->elf, DWARF_C_READ, NULL);
  if (mod->dw == NULL)
    {
      int err = INTUSE(dwarf_errno) ();
      return err == DWARF_E_NO_DWARF ? DWFL_E_NO_DWARF : DWFL_E (LIBDW, err);
    }

  /* Until we have iterated through all CU's, we might do lazy lookups.  */
  mod->lazycu = 1;

  return DWFL_E_NOERROR;
}

/* Try to start up libdw on either the main file or the debuginfo file.  */
static void
find_dw (Dwfl_Module *mod)
{
  if (mod->dw != NULL		/* Already done.  */
      || mod->dwerr != DWFL_E_NOERROR) /* Cached previous failure.  */
    return;

  __libdwfl_getelf (mod);
  mod->dwerr = mod->elferr;
  if (mod->dwerr != DWFL_E_NOERROR)
    return;

  /* First see if the main ELF file has the debugging information.  */
  mod->dwerr = load_dw (mod, &mod->main);
  switch (mod->dwerr)
    {
    case DWFL_E_NOERROR:
      mod->debug.elf = mod->main.elf;
      mod->debug.address_sync = mod->main.address_sync;
      return;

    case DWFL_E_NO_DWARF:
      break;

    default:
      goto canonicalize;
    }

  /* Now we have to look for a separate debuginfo file.  */
  mod->dwerr = find_debuginfo (mod);
  switch (mod->dwerr)
    {
    case DWFL_E_NOERROR:
      mod->dwerr = load_dw (mod, &mod->debug);
      break;

    case DWFL_E_CB:		/* The find_debuginfo hook failed.  */
      mod->dwerr = DWFL_E_NO_DWARF;
      return;

    default:
      break;
    }

 canonicalize:
  mod->dwerr = __libdwfl_canon_error (mod->dwerr);
}

Dwarf *
dwfl_module_getdwarf (Dwfl_Module *mod, Dwarf_Addr *bias)
{
  if (mod == NULL)
    return NULL;

  find_dw (mod);
  if (mod->dwerr == DWFL_E_NOERROR)
    {
      /* If dwfl_module_getelf was used previously, then partial apply
	 relocation to miscellaneous sections in the debug file too.  */
      if (mod->e_type == ET_REL
	  && mod->main.relocated && ! mod->debug.relocated)
	{
	  mod->debug.relocated = true;
	  if (mod->debug.elf != mod->main.elf)
	    (void) __libdwfl_relocate (mod, mod->debug.elf, false);
	}

      *bias = dwfl_adjusted_dwarf_addr (mod, 0);
      return mod->dw;
    }

  __libdwfl_seterrno (mod->dwerr);
  return NULL;
}
INTDEF (dwfl_module_getdwarf)

int
dwfl_module_getsymtab (Dwfl_Module *mod)
{
  if (mod == NULL)
    return -1;

  find_symtab (mod);
  if (mod->symerr == DWFL_E_NOERROR)
    return mod->syments;

  __libdwfl_seterrno (mod->symerr);
  return -1;
}
INTDEF (dwfl_module_getsymtab)
