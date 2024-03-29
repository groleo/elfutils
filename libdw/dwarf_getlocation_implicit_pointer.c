/* Return associated attribute for DW_OP_GNU_implicit_pointer.
   Copyright (C) 2010 Red Hat, Inc.
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

#include "libdwP.h"
#include <dwarf.h>


int
dwarf_getlocation_implicit_pointer (attr, op, result)
     Dwarf_Attribute *attr;
     const Dwarf_Op *op;
     Dwarf_Attribute *result;
{
  if (attr == NULL)
    return -1;

  if (unlikely (op->atom != DW_OP_GNU_implicit_pointer))
    {
      __libdw_seterrno (DWARF_E_INVALID_ACCESS);
      return -1;
    }

  Dwarf_Die die;
  if (__libdw_offdie (attr->cu->dbg, op->number, &die,
		      attr->cu->type_offset != 0) == NULL)
    return -1;

  if (INTUSE(dwarf_attr) (&die, DW_AT_location, result) == NULL
      && INTUSE(dwarf_attr) (&die, DW_AT_const_value, result) == NULL)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  return 0;
}
