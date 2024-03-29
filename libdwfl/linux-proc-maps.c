/* Standard libdwfl callbacks for debugging a live Linux process.
   Copyright (C) 2005-2010 Red Hat, Inc.
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
#include <inttypes.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <endian.h>


#define PROCMAPSFMT	"/proc/%d/maps"
#define PROCMEMFMT	"/proc/%d/mem"
#define PROCAUXVFMT	"/proc/%d/auxv"


/* Search /proc/PID/auxv for the AT_SYSINFO_EHDR tag.  */

static int
grovel_auxv (pid_t pid, Dwfl *dwfl, GElf_Addr *sysinfo_ehdr)
{
  char *fname;
  if (asprintf (&fname, PROCAUXVFMT, pid) < 0)
    return ENOMEM;

  int fd = open64 (fname, O_RDONLY);
  free (fname);
  if (fd < 0)
    return errno == ENOENT ? 0 : errno;

  ssize_t nread;
  do
    {
      union
      {
	char buffer[sizeof (long int) * 2 * 64];
	Elf64_auxv_t a64[sizeof (long int) * 2 * 64 / sizeof (Elf64_auxv_t)];
	Elf32_auxv_t a32[sizeof (long int) * 2 * 32 / sizeof (Elf32_auxv_t)];
      } d;
      nread = read (fd, &d, sizeof d);
      if (nread > 0)
	{
	  switch (sizeof (long int))
	    {
	    case 4:
	      for (size_t i = 0; (char *) &d.a32[i] < &d.buffer[nread]; ++i)
		if (d.a32[i].a_type == AT_SYSINFO_EHDR)
		  {
		    *sysinfo_ehdr = d.a32[i].a_un.a_val;
		    if (dwfl->segment_align > 1)
		      {
			nread = 0;
			break;
		      }
		  }
		else if (d.a32[i].a_type == AT_PAGESZ
			 && dwfl->segment_align <= 1)
		  dwfl->segment_align = d.a32[i].a_un.a_val;
	      break;
	    case 8:
	      for (size_t i = 0; (char *) &d.a64[i] < &d.buffer[nread]; ++i)
		if (d.a64[i].a_type == AT_SYSINFO_EHDR)
		  {
		    *sysinfo_ehdr = d.a64[i].a_un.a_val;
		    if (dwfl->segment_align > 1)
		      {
			nread = 0;
			break;
		      }
		  }
		else if (d.a64[i].a_type == AT_PAGESZ
			 && dwfl->segment_align <= 1)
		  dwfl->segment_align = d.a64[i].a_un.a_val;
	      break;
	    default:
	      abort ();
	      break;
	    }
	}
    }
  while (nread > 0);

  close (fd);

  return nread < 0 ? errno : 0;
}

static int
proc_maps_report (Dwfl *dwfl, FILE *f, GElf_Addr sysinfo_ehdr, pid_t pid)
{
  unsigned int last_dmajor = -1, last_dminor = -1;
  uint64_t last_ino = -1;
  char *last_file = NULL;
  Dwarf_Addr low = 0, high = 0;

  inline bool report (void)
    {
      if (last_file != NULL)
	{
	  Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, last_file,
							 low, high);
	  free (last_file);
	  last_file = NULL;
	  if (unlikely (mod == NULL))
	    return true;
	}
      return false;
    }

  char *line = NULL;
  size_t linesz;
  ssize_t len;
  while ((len = getline (&line, &linesz, f)) > 0)
    {
      if (line[len - 1] == '\n')
	line[len - 1] = '\0';

      Dwarf_Addr start, end, offset;
      unsigned int dmajor, dminor;
      uint64_t ino;
      int nread = -1;
      if (sscanf (line, "%" PRIx64 "-%" PRIx64 " %*s %" PRIx64
		  " %x:%x %" PRIi64 " %n",
		  &start, &end, &offset, &dmajor, &dminor, &ino, &nread) < 6
	  || nread <= 0)
	{
	  free (line);
	  return ENOEXEC;
	}

      /* If this is the special mapping AT_SYSINFO_EHDR pointed us at,
	 report the last one and then this special one.  */
      if (start == sysinfo_ehdr && start != 0)
	{
	  if (report ())
	    {
	    bad_report:
	      free (line);
	      fclose (f);
	      return -1;
	    }

	  low = start;
	  high = end;
	  if (asprintf (&last_file, "[vdso: %d]", (int) pid) < 0
	      || report ())
	    goto bad_report;
	}

      char *file = line + nread + strspn (line + nread, " \t");
      if (file[0] == '\0' || (ino == 0 && dmajor == 0 && dminor == 0))
	/* This line doesn't indicate a file mapping.  */
	continue;

      if (last_file != NULL
	  && ino == last_ino && dmajor == last_dmajor && dminor == last_dminor)
	{
	  /* This is another portion of the same file's mapping.  */
	  assert (!strcmp (last_file, file));
	  high = end;
	}
      else
	{
	  /* This is a different file mapping.  Report the last one.  */
	  if (report ())
	    goto bad_report;
	  low = start;
	  high = end;
	  last_file = strdup (file);
	  last_ino = ino;
	  last_dmajor = dmajor;
	  last_dminor = dminor;
	}
    }
  free (line);

  int result = ferror_unlocked (f) ? errno : feof_unlocked (f) ? 0 : ENOEXEC;

  /* Report the final one.  */
  bool lose = report ();

  return result != 0 ? result : lose ? -1 : 0;
}

int
dwfl_linux_proc_maps_report (Dwfl *dwfl, FILE *f)
{
  return proc_maps_report (dwfl, f, 0, 0);
}
INTDEF (dwfl_linux_proc_maps_report)

int
dwfl_linux_proc_report (Dwfl *dwfl, pid_t pid)
{
  if (dwfl == NULL)
    return -1;

  /* We'll notice the AT_SYSINFO_EHDR address specially when we hit it.  */
  GElf_Addr sysinfo_ehdr = 0;
  int result = grovel_auxv (pid, dwfl, &sysinfo_ehdr);
  if (result != 0)
    return result;

  char *fname;
  if (asprintf (&fname, PROCMAPSFMT, pid) < 0)
    return ENOMEM;

  FILE *f = fopen (fname, "r");
  free (fname);
  if (f == NULL)
    return errno;

  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  result = proc_maps_report (dwfl, f, sysinfo_ehdr, pid);

  fclose (f);

  return result;
}
INTDEF (dwfl_linux_proc_report)

static ssize_t
read_proc_memory (void *arg, void *data, GElf_Addr address,
		  size_t minread, size_t maxread)
{
  const int fd = *(const int *) arg;
  ssize_t nread = pread64 (fd, data, maxread, (off64_t) address);
  /* Some kernels don't actually let us do this read, ignore those errors.  */
  if (nread < 0 && (errno == EINVAL || errno == EPERM))
    return 0;
  if (nread > 0 && (size_t) nread < minread)
    nread = 0;
  return nread;
}

extern Elf *elf_from_remote_memory (GElf_Addr ehdr_vma,
				    GElf_Addr *loadbasep,
				    ssize_t (*read_memory) (void *arg,
							    void *data,
							    GElf_Addr address,
							    size_t minread,
							    size_t maxread),
				    void *arg);


/* Dwfl_Callbacks.find_elf */

int
dwfl_linux_proc_find_elf (Dwfl_Module *mod __attribute__ ((unused)),
			  void **userdata __attribute__ ((unused)),
			  const char *module_name, Dwarf_Addr base,
			  char **file_name, Elf **elfp)
{
  if (module_name[0] == '/')
    {
      int fd = open64 (module_name, O_RDONLY);
      if (fd >= 0)
	{
	  *file_name = strdup (module_name);
	  if (*file_name == NULL)
	    {
	      close (fd);
	      return ENOMEM;
	    }
	}
      return fd;
    }

  int pid;
  if (sscanf (module_name, "[vdso: %d]", &pid) == 1)
    {
      /* Special case for in-memory ELF image.  */

      char *fname;
      if (asprintf (&fname, PROCMEMFMT, pid) < 0)
	return -1;

      int fd = open64 (fname, O_RDONLY);
      free (fname);
      if (fd < 0)
	return -1;

      *elfp = elf_from_remote_memory (base, NULL, &read_proc_memory, &fd);

      close (fd);

      *file_name = NULL;
      return -1;
    }

  abort ();
  return -1;
}
INTDEF (dwfl_linux_proc_find_elf)
