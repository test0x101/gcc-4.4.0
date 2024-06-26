/* Target definitions for GCC for Intel 80386 running Interix V3.
   Copyright (C) 2001, 2007 Free Software Foundation, Inc.
   Contributed by Douglas B. Rupp (rupp@gnat.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE}"
