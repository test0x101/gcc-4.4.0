/* Scalar evolution detector.
   Copyright (C) 2003, 2004, 2005, 2007, 2008 Free Software Foundation, Inc.
   Contributed by Sebastian Pop <s.pop@laposte.net>

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

#ifndef GCC_TREE_SCALAR_EVOLUTION_H
#define GCC_TREE_SCALAR_EVOLUTION_H

extern tree number_of_latch_executions(struct loop *);
extern tree number_of_exit_cond_executions(struct loop *);
extern gimple get_loop_exit_condition(const struct loop *);

extern void scev_initialize(void);
extern void scev_reset(void);
extern void scev_finalize(void);
extern tree analyze_scalar_evolution(struct loop *, tree);
extern tree instantiate_scev(basic_block, struct loop *, tree);
extern tree resolve_mixers(struct loop *, tree);
extern void gather_stats_on_scev_database(void);
extern void scev_analysis(void);
unsigned int scev_const_prop(void);

bool expression_expensive_p(tree);
extern bool simple_iv(struct loop *, struct loop *, tree, affine_iv *, bool);

/* Returns the basic block preceding LOOP or ENTRY_BLOCK_PTR when the
   loop is function's body.  */

static inline basic_block block_before_loop(loop_p loop) {
  edge preheader = loop_preheader_edge(loop);
  return (preheader ? preheader->src : ENTRY_BLOCK_PTR);
}

/* Analyze all the parameters of the chrec that were left under a
   symbolic form.  LOOP is the loop in which symbolic names have to
   be analyzed and instantiated.  */

static inline tree instantiate_parameters(struct loop *loop, tree chrec) {
  return instantiate_scev(block_before_loop(loop), loop, chrec);
}

/* Returns the loop of the polynomial chrec CHREC.  */

static inline struct loop *get_chrec_loop(const_tree chrec) {
  return get_loop(CHREC_VARIABLE(chrec));
}

#endif /* GCC_TREE_SCALAR_EVOLUTION_H  */
