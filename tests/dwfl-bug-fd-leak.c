/* Test program for libdwfl file decriptors leakage.
   Copyright (C) 2007 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <locale.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <dwarf.h>
#include <sys/resource.h>
#include ELFUTILS_HEADER(dwfl)


static Dwfl *
elfutils_open (pid_t pid, Dwarf_Addr address)
{
  static char *debuginfo_path;
  static const Dwfl_Callbacks proc_callbacks =
    {
      .find_debuginfo = dwfl_standard_find_debuginfo,
      .debuginfo_path = &debuginfo_path,

      .find_elf = dwfl_linux_proc_find_elf,
    };
  Dwfl *dwfl = dwfl_begin (&proc_callbacks);
  if (dwfl == NULL)
    error (2, 0, "dwfl_begin: %s", dwfl_errmsg (-1));

  int result = dwfl_linux_proc_report (dwfl, pid);
  if (result < 0)
    error (2, 0, "dwfl_linux_proc_report: %s", dwfl_errmsg (-1));
  else if (result > 0)
    error (2, result, "dwfl_linux_proc_report");

  if (dwfl_report_end (dwfl, NULL, NULL) != 0)
    error (2, 0, "dwfl_report_end: %s", dwfl_errmsg (-1));

  Dwarf_Addr bias;
  Dwarf *dbg = dwfl_addrdwarf (dwfl, address, &bias);
  if (dbg != NULL)
    {
      Elf *elf = dwarf_getelf (dbg);
      if (elf == NULL)
	error (2, 0, "dwarf_getelf: %s", dwarf_errmsg (-1));
    }
  else
    {
      Elf *elf = dwfl_module_getelf (dwfl_addrmodule (dwfl, address), &bias);
      if (elf == NULL)
	error (2, 0, "dwfl_module_getelf: %s", dwfl_errmsg (-1));
    }

  return dwfl;
}

static void
elfutils_close (Dwfl *dwfl)
{
  dwfl_end (dwfl);
}

int
main (void)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  struct rlimit fd_limit = { .rlim_cur = 32, .rlim_max = 32 };
  if (setrlimit (RLIMIT_NOFILE, &fd_limit) < 0)
    error (2, errno, "setrlimit");

  for (int i = 0; i < 5000; ++i)
    {
      Dwfl *dwfl = elfutils_open (getpid (), (Dwarf_Addr) main);
      elfutils_close (dwfl);
    }

  return 0;
}