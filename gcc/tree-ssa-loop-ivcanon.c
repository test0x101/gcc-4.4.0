/* Induction variable canonicalization.
   Copyright (C) 2004, 2005, 2007, 2008 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

/* This pass detects the loops that iterate a constant number of times,
   adds a canonical induction variable (step -1, tested against 0)
   and replaces the exit test.  This enables the less powerful rtl
   level analysis to use this information.

   This might spoil the code in some cases (by increasing register pressure).
   Note that in the case the new variable is not needed, ivopts will get rid
   of it, so it might only be a problem when there are no other linear induction
   variables.  In that case the created optimization possibilities are likely
   to pay up.

   Additionally in case we detect that it is beneficial to unroll the
   loop completely, we do it right here to expose the optimization
   possibilities to the following passes.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "cfgloop.h"
#include "tree-pass.h"
#include "ggc.h"
#include "tree-chrec.h"
#include "tree-scalar-evolution.h"
#include "params.h"
#include "flags.h"
#include "tree-inline.h"

/* Specifies types of loops that may be unrolled.  */

enum unroll_level {
  UL_SINGLE_ITER, /* Only loops that exit immediately in the first
                     iteration.  */
  UL_NO_GROWTH,   /* Only loops whose unrolling will not cause increase
                     of code size.  */
  UL_ALL          /* All suitable loops.  */
};

/* Adds a canonical induction variable to LOOP iterating NITER times.  EXIT
   is the exit edge whose condition is replaced.  */

static void create_canonical_iv(struct loop *loop, edge exit, tree niter) {
  edge in;
  tree type, var;
  gimple cond;
  gimple_stmt_iterator incr_at;
  enum tree_code cmp;

  if (dump_file && (dump_flags & TDF_DETAILS)) {
    fprintf(dump_file, "Added canonical iv to loop %d, ", loop->num);
    print_generic_expr(dump_file, niter, TDF_SLIM);
    fprintf(dump_file, " iterations.\n");
  }

  cond = last_stmt(exit->src);
  in = EDGE_SUCC(exit->src, 0);
  if (in == exit)
    in = EDGE_SUCC(exit->src, 1);

  /* Note that we do not need to worry about overflows, since
     type of niter is always unsigned and all comparisons are
     just for equality/nonequality -- i.e. everything works
     with a modulo arithmetics.  */

  type = TREE_TYPE(niter);
  niter = fold_build2(PLUS_EXPR, type, niter, build_int_cst(type, 1));
  incr_at = gsi_last_bb(in->src);
  create_iv(niter, build_int_cst(type, -1), NULL_TREE, loop, &incr_at, false,
            NULL, &var);

  cmp = (exit->flags & EDGE_TRUE_VALUE) ? EQ_EXPR : NE_EXPR;
  gimple_cond_set_code(cond, cmp);
  gimple_cond_set_lhs(cond, var);
  gimple_cond_set_rhs(cond, build_int_cst(type, 0));
  update_stmt(cond);
}

/* Computes an estimated number of insns in LOOP, weighted by WEIGHTS.  */

unsigned tree_num_loop_insns(struct loop *loop, eni_weights *weights) {
  basic_block *body = get_loop_body(loop);
  gimple_stmt_iterator gsi;
  unsigned size = 1, i;

  for (i = 0; i < loop->num_nodes; i++)
    for (gsi = gsi_start_bb(body[i]); !gsi_end_p(gsi); gsi_next(&gsi))
      size += estimate_num_insns(gsi_stmt(gsi), weights);
  free(body);

  return size;
}

/* Estimate number of insns of completely unrolled loop.  We assume
   that the size of the unrolled loop is decreased in the
   following way (the numbers of insns are based on what
   estimate_num_insns returns for appropriate statements):

   1) exit condition gets removed (2 insns)
   2) increment of the control variable gets removed (2 insns)
   3) All remaining statements are likely to get simplified
      due to constant propagation.  Hard to estimate; just
      as a heuristics we decrease the rest by 1/3.

   NINSNS is the number of insns in the loop before unrolling.
   NUNROLL is the number of times the loop is unrolled.  */

static unsigned HOST_WIDE_INT estimated_unrolled_size(
    unsigned HOST_WIDE_INT ninsns, unsigned HOST_WIDE_INT nunroll) {
  HOST_WIDE_INT unr_insns = 2 * ((HOST_WIDE_INT)ninsns - 4) / 3;
  if (unr_insns <= 0)
    unr_insns = 1;
  unr_insns *= (nunroll + 1);

  return unr_insns;
}

/* Tries to unroll LOOP completely, i.e. NITER times.
   UL determines which loops we are allowed to unroll.
   EXIT is the exit of the loop that should be eliminated.  */

static bool try_unroll_loop_completely(struct loop *loop, edge exit, tree niter,
                                       enum unroll_level ul) {
  unsigned HOST_WIDE_INT n_unroll, ninsns, max_unroll, unr_insns;
  gimple cond;

  if (loop->inner)
    return false;

  if (!host_integerp(niter, 1))
    return false;
  n_unroll = tree_low_cst(niter, 1);

  max_unroll = PARAM_VALUE(PARAM_MAX_COMPLETELY_PEEL_TIMES);
  if (n_unroll > max_unroll)
    return false;

  if (n_unroll) {
    if (ul == UL_SINGLE_ITER)
      return false;

    ninsns = tree_num_loop_insns(loop, &eni_size_weights);

    unr_insns = estimated_unrolled_size(ninsns, n_unroll);
    if (dump_file && (dump_flags & TDF_DETAILS)) {
      fprintf(dump_file, "  Loop size: %d\n", (int)ninsns);
      fprintf(dump_file, "  Estimated size after unrolling: %d\n",
              (int)unr_insns);
    }

    if (unr_insns > ninsns &&
        (unr_insns >
         (unsigned)PARAM_VALUE(PARAM_MAX_COMPLETELY_PEELED_INSNS))) {
      if (dump_file && (dump_flags & TDF_DETAILS))
        fprintf(dump_file,
                "Not unrolling loop %d "
                "(--param max-completely-peeled-insns limit reached).\n",
                loop->num);
      return false;
    }

    if (ul == UL_NO_GROWTH && unr_insns > ninsns) {
      if (dump_file && (dump_flags & TDF_DETAILS))
        fprintf(dump_file, "Not unrolling loop %d.\n", loop->num);
      return false;
    }
  }

  if (n_unroll) {
    sbitmap wont_exit;
    edge e;
    unsigned i;
    VEC(edge, heap) *to_remove = NULL;

    initialize_original_copy_tables();
    wont_exit = sbitmap_alloc(n_unroll + 1);
    sbitmap_ones(wont_exit);
    RESET_BIT(wont_exit, 0);

    if (!gimple_duplicate_loop_to_header_edge(
            loop, loop_preheader_edge(loop), n_unroll, wont_exit, exit,
            &to_remove, DLTHE_FLAG_UPDATE_FREQ | DLTHE_FLAG_COMPLETTE_PEEL)) {
      free_original_copy_tables();
      free(wont_exit);
      return false;
    }

    for (i = 0; VEC_iterate(edge, to_remove, i, e); i++) {
      bool ok = remove_path(e);
      gcc_assert(ok);
    }

    VEC_free(edge, heap, to_remove);
    free(wont_exit);
    free_original_copy_tables();
  }

  cond = last_stmt(exit->src);
  if (exit->flags & EDGE_TRUE_VALUE)
    gimple_cond_make_true(cond);
  else
    gimple_cond_make_false(cond);
  update_stmt(cond);
  update_ssa(TODO_update_ssa);

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf(dump_file, "Unrolled loop %d completely.\n", loop->num);

  return true;
}

/* Adds a canonical induction variable to LOOP if suitable.
   CREATE_IV is true if we may create a new iv.  UL determines
   which loops we are allowed to completely unroll.  If TRY_EVAL is true, we try
   to determine the number of iterations of a loop by direct evaluation.
   Returns true if cfg is changed.  */

static bool canonicalize_loop_induction_variables(struct loop *loop,
                                                  bool create_iv,
                                                  enum unroll_level ul,
                                                  bool try_eval) {
  edge exit = NULL;
  tree niter;

  niter = number_of_latch_executions(loop);
  if (TREE_CODE(niter) == INTEGER_CST) {
    exit = single_exit(loop);
    if (!just_once_each_iteration_p(loop, exit->src))
      return false;
  } else {
    /* If the loop has more than one exit, try checking all of them
       for # of iterations determinable through scev.  */
    if (!single_exit(loop))
      niter = find_loop_niter(loop, &exit);

    /* Finally if everything else fails, try brute force evaluation.  */
    if (try_eval &&
        (chrec_contains_undetermined(niter) || TREE_CODE(niter) != INTEGER_CST))
      niter = find_loop_niter_by_eval(loop, &exit);

    if (chrec_contains_undetermined(niter) || TREE_CODE(niter) != INTEGER_CST)
      return false;
  }

  if (dump_file && (dump_flags & TDF_DETAILS)) {
    fprintf(dump_file, "Loop %d iterates ", loop->num);
    print_generic_expr(dump_file, niter, TDF_SLIM);
    fprintf(dump_file, " times.\n");
  }

  if (try_unroll_loop_completely(loop, exit, niter, ul))
    return true;

  if (create_iv)
    create_canonical_iv(loop, exit, niter);

  return false;
}

/* The main entry point of the pass.  Adds canonical induction variables
   to the suitable loops.  */

unsigned int canonicalize_induction_variables(void) {
  loop_iterator li;
  struct loop *loop;
  bool changed = false;

  FOR_EACH_LOOP(li, loop, 0) {
    changed |=
        canonicalize_loop_induction_variables(loop, true, UL_SINGLE_ITER, true);
  }

  /* Clean up the information about numbers of iterations, since brute force
     evaluation could reveal new information.  */
  scev_reset();

  if (changed)
    return TODO_cleanup_cfg;
  return 0;
}

/* Unroll LOOPS completely if they iterate just few times.  Unless
   MAY_INCREASE_SIZE is true, perform the unrolling only if the
   size of the code does not increase.  */

unsigned int tree_unroll_loops_completely(bool may_increase_size,
                                          bool unroll_outer) {
  loop_iterator li;
  struct loop *loop;
  bool changed;
  enum unroll_level ul;

  do {
    changed = false;

    FOR_EACH_LOOP(li, loop, LI_ONLY_INNERMOST) {
      if (may_increase_size &&
          optimize_loop_for_speed_p(loop)
          /* Unroll outermost loops only if asked to do so or they do
             not cause code growth.  */
          && (unroll_outer || loop_outer(loop_outer(loop))))
        ul = UL_ALL;
      else
        ul = UL_NO_GROWTH;
      changed |= canonicalize_loop_induction_variables(loop, false, ul,
                                                       !flag_tree_loop_ivcanon);
    }

    if (changed) {
      /* This will take care of removing completely unrolled loops
         from the loop structures so we can continue unrolling now
         innermost loops.  */
      if (cleanup_tree_cfg())
        update_ssa(TODO_update_ssa_only_virtuals);

      /* Clean up the information about numbers of iterations, since
         complete unrolling might have invalidated it.  */
      scev_reset();
    }
  } while (changed);

  return 0;
}

/* Checks whether LOOP is empty.  */

static bool empty_loop_p(struct loop *loop) {
  edge exit;
  struct tree_niter_desc niter;
  basic_block *body;
  gimple_stmt_iterator gsi;
  unsigned i;

  /* If the loop has multiple exits, it is too hard for us to handle.
     Similarly, if the exit is not dominating, we cannot determine
     whether the loop is not infinite.  */
  exit = single_dom_exit(loop);
  if (!exit)
    return false;

  /* The loop must be finite.  */
  if (!number_of_iterations_exit(loop, exit, &niter, false))
    return false;

  /* Values of all loop exit phi nodes must be invariants.  */
  for (gsi = gsi_start(phi_nodes(exit->dest)); !gsi_end_p(gsi);
       gsi_next(&gsi)) {
    gimple phi = gsi_stmt(gsi);
    tree def;

    if (!is_gimple_reg(PHI_RESULT(phi)))
      continue;

    def = PHI_ARG_DEF_FROM_EDGE(phi, exit);

    if (!expr_invariant_in_loop_p(loop, def))
      return false;
  }

  /* And there should be no memory modifying or from other reasons
     unremovable statements.  */
  body = get_loop_body(loop);
  for (i = 0; i < loop->num_nodes; i++) {
    /* Irreducible region might be infinite.  */
    if (body[i]->flags & BB_IRREDUCIBLE_LOOP) {
      free(body);
      return false;
    }

    for (gsi = gsi_start_bb(body[i]); !gsi_end_p(gsi); gsi_next(&gsi)) {
      gimple stmt = gsi_stmt(gsi);

      if (!ZERO_SSA_OPERANDS(stmt, SSA_OP_VIRTUAL_DEFS) ||
          gimple_has_volatile_ops(stmt)) {
        free(body);
        return false;
      }

      /* Also, asm statements and calls may have side effects and we
         cannot change the number of times they are executed.  */
      switch (gimple_code(stmt)) {
        case GIMPLE_CALL:
          if (gimple_has_side_effects(stmt)) {
            free(body);
            return false;
          }
          break;

        case GIMPLE_ASM:
          /* We cannot remove volatile assembler.  */
          if (gimple_asm_volatile_p(stmt)) {
            free(body);
            return false;
          }
          break;

        default:
          break;
      }
    }
  }
  free(body);

  return true;
}

/* Remove LOOP by making it exit in the first iteration.  */

static void remove_empty_loop(struct loop *loop) {
  edge exit = single_dom_exit(loop), non_exit;
  gimple cond_stmt = last_stmt(exit->src);
  basic_block *body;
  unsigned n_before, freq_in, freq_h;
  gcov_type exit_count = exit->count;

  if (dump_file)
    fprintf(dump_file, "Removing empty loop %d\n", loop->num);

  non_exit = EDGE_SUCC(exit->src, 0);
  if (non_exit == exit)
    non_exit = EDGE_SUCC(exit->src, 1);

  if (exit->flags & EDGE_TRUE_VALUE)
    gimple_cond_make_true(cond_stmt);
  else
    gimple_cond_make_false(cond_stmt);
  update_stmt(cond_stmt);

  /* Let us set the probabilities of the edges coming from the exit block.  */
  exit->probability = REG_BR_PROB_BASE;
  non_exit->probability = 0;
  non_exit->count = 0;

  /* Update frequencies and counts.  Everything before
     the exit needs to be scaled FREQ_IN/FREQ_H times,
     where FREQ_IN is the frequency of the entry edge
     and FREQ_H is the frequency of the loop header.
     Everything after the exit has zero frequency.  */
  freq_h = loop->header->frequency;
  freq_in = EDGE_FREQUENCY(loop_preheader_edge(loop));
  if (freq_h != 0) {
    body = get_loop_body_in_dom_order(loop);
    for (n_before = 1; n_before <= loop->num_nodes; n_before++)
      if (body[n_before - 1] == exit->src)
        break;
    scale_bbs_frequencies_int(body, n_before, freq_in, freq_h);
    scale_bbs_frequencies_int(body + n_before, loop->num_nodes - n_before, 0,
                              1);
    free(body);
  }

  /* Number of executions of exit is not changed, thus we need to restore
     the original value.  */
  exit->count = exit_count;
}

/* Removes LOOP if it is empty.  Returns true if LOOP is removed.  CHANGED
   is set to true if LOOP or any of its subloops is removed.  */

static bool try_remove_empty_loop(struct loop *loop, bool *changed) {
  bool nonempty_subloop = false;
  struct loop *sub;

  /* First, all subloops must be removed.  */
  for (sub = loop->inner; sub; sub = sub->next)
    nonempty_subloop |= !try_remove_empty_loop(sub, changed);

  if (nonempty_subloop || !empty_loop_p(loop))
    return false;

  remove_empty_loop(loop);
  *changed = true;
  return true;
}

/* Remove the empty loops.  */

unsigned int remove_empty_loops(void) {
  bool changed = false;
  struct loop *loop;

  for (loop = current_loops->tree_root->inner; loop; loop = loop->next)
    try_remove_empty_loop(loop, &changed);

  if (changed) {
    scev_reset();
    return TODO_cleanup_cfg;
  }
  return 0;
}
