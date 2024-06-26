/* Exported functions from emit-rtl.c
   Copyright (C) 2004, 2007, 2008 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_EMIT_RTL_H
#define GCC_EMIT_RTL_H

/* Set the alias set of MEM to SET.  */
extern void set_mem_alias_set(rtx, alias_set_type);

/* Set the alignment of MEM to ALIGN bits.  */
extern void set_mem_align(rtx, unsigned int);

/* Set the expr for MEM to EXPR.  */
extern void set_mem_expr(rtx, tree);

/* Set the offset for MEM to OFFSET.  */
extern void set_mem_offset(rtx, rtx);

/* Set the size for MEM to SIZE.  */
extern void set_mem_size(rtx, rtx);

/* Set the attributes for MEM appropriate for a spill slot.  */
extern void set_mem_attrs_for_spill(rtx);
extern tree get_spill_slot_decl(bool);

/* Return a memory reference like MEMREF, but with its address changed to
   ADDR.  The caller is asserting that the actual piece of memory pointed
   to is the same, just the form of the address is being changed, such as
   by putting something into a register.  */
extern rtx replace_equiv_address(rtx, rtx);

/* Likewise, but the reference is not required to be valid.  */
extern rtx replace_equiv_address_nv(rtx, rtx);

#endif /* GCC_EMIT_RTL_H */
