#! /bin/sh
# Copyright (C) 2007, 2008 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh

# Run x86-64 test.
case "`uname -m`" in
  x86_64)
    tempfiles testfile45.o
    testfiles testfile45.S testfile45.expect
    gcc -m64 -c -o testfile45.o testfile45.S
    testrun_compare ../src/objdump -d testfile45.o < testfile45.expect
    ;;
esac
