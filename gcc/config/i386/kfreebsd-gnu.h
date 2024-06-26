/* Definitions for Intel 386 running kFreeBSD-based GNU systems with ELF format
   Copyright (C) 2004, 2007
   Free Software Foundation, Inc.
   Contributed by Robert Millan.

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

#undef LINK_EMULATION
#define LINK_EMULATION "elf_i386_fbsd"
#undef REG_NAME
#define REG_NAME(reg) sc_##reg
