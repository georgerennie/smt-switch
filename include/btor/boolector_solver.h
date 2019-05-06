#ifndef SMT_BOOLECTOR_SOLVER_H
#define SMT_BOOLECTOR_SOLVER_H

#include <memory>
#include <string>
#include <vector>

#include "exceptions.h"
#include "boolector_function.h"
#include "boolector_op.h"
#include "boolector_sort.h"
#include "boolector_term.h"
#include "solver.h"
#include "sort.h"

namespace smt
{
  /**
     Boolector Solver implementation
   */
  class BoolectorSolver : public AbsSolver
  {
  public:
    // might have to use std::unique_ptr<Btor>(boolector_new) and move it?
    BoolectorSolver() : btor(boolector_new()) {};
    BoolectorSolver(const BoolectorSolver&) = delete;
    BoolectorSolver& operator=(const BoolectorSolver&) = delete;
    ~BoolectorSolver() { boolector_delete(btor); };
    Sort declare_sort(const std::string name, unsigned int arity) const override
    {
      throw IncorrectUsageException("Can't declare sorts with Boolector");
    }
    Term declare_const(const std::string name, Sort sort) const override
    {
      // TODO handle arrays correctly (need boolector_array instead of boolector_var)
      std::shared_ptr<BoolectorSortBase> bs = std::static_pointer_cast<BoolectorSortBase>(sort);
      Term term(new BoolectorTerm(btor,
                                  boolector_var(btor, bs->sort, name.c_str()),
                                  std::vector<Term>{},
                                  VAR));
      return term;
    }
    // TODO implement declare_fun
    Term make_const(unsigned int i, Sort sort) const override
    {
      std::shared_ptr<BoolectorSortBase> bs = std::static_pointer_cast<BoolectorSortBase>(sort);
      Term term(new BoolectorTerm(btor,
                                  boolector_int(btor, i, bs->sort),
                                  std::vector<Term>{},
                                  CONST
                                  ));
      return term;
    }
    void assert_formula(Term& t) const override
    {
      std::shared_ptr<BoolectorTerm> bt = std::static_pointer_cast<BoolectorTerm>(t);
      boolector_assert(btor, bt->node);
    }
    bool check_sat() const override { return boolector_sat(btor); };
    // TODO: Implement this
    Term get_value(Term& t) const override {}
    /* { */
    /*  Need to use boolector_*_assignment which returns a c str that we need to turn back into a term */
    /*   Term t; */
    /*   Kind k = t->get_sort()->get_kind(); */
    /*   if (k == BV) */
    /*   { */
    /*   } */
    /*   else if (k == ARRAY) */
    /*   { */
    /*   } */
    /*   else if (k == UNINTERPRETED) */
    /*   { */
    /*   } */
    /*   else */
    /*   { */
    /*     std::string msg("Can't get value for term with kind = "); */
    /*     mst += to_string(k); */
    /*     throw IncorrectUsageException(msg.c_str()); */
    /*   } */
    /* }; */
    Sort construct_sort(Kind k) const override
    {
      if (k == BOOL)
      {
        Sort s(new BoolectorBVSort(btor, boolector_bool_sort(btor), 1));
        return s;
      }
      else
      {
        std::string msg("Boolector does not support ");
        msg += to_string(k);
        throw NotImplementedException(msg.c_str());
      }
    }
    Sort construct_sort(Kind k, unsigned int size) const override
    {
      if (k == BV)
        {
          Sort s(new BoolectorBVSort(btor, boolector_bitvec_sort(btor, size), size));
          return s;
        }
      else
        {
          std::string msg("Can't create Kind ");
          msg += to_string(k);
          msg += " with int argument.";
          throw IncorrectUsageException(msg.c_str());
        }
    }
    Sort construct_sort(Kind k, Sort idxsort, Sort elemsort) const override
    {
      if (k == ARRAY)
      {
        std::shared_ptr<BoolectorSortBase> btor_idxsort = std::static_pointer_cast<BoolectorSortBase>(idxsort);
        std::shared_ptr<BoolectorSortBase> btor_elemsort = std::static_pointer_cast<BoolectorSortBase>(elemsort);
        BoolectorSort bs = boolector_array_sort(btor, btor_idxsort->sort, btor_elemsort->sort);
        Sort s(new BoolectorArraySort(btor, bs, idxsort, elemsort));
        return s;
      }
      else
      {
        std::string msg("Can't create Kind ");
        msg += to_string(k);
        msg += " with two sort arguments.";
        throw IncorrectUsageException(msg.c_str());
      }
    }
    Sort construct_sort(Kind k, std::vector<Sort> sorts, Sort sort) const override
    {
      if (k == UNINTERPRETED)
        {
          int arity = sorts.size();
          std::vector<BoolectorSort> btor_sorts(arity);
          for (auto s : sorts)
          {
            std::shared_ptr<BoolectorSortBase> bs = std::static_pointer_cast<BoolectorSortBase>(s);
            btor_sorts.push_back(bs->sort);
          }
          std::shared_ptr<BoolectorSortBase> btor_sort = std::static_pointer_cast<BoolectorSortBase>(sort);
          BoolectorSort btor_fun_sort = boolector_fun_sort(btor, &btor_sorts[0], arity, btor_sort->sort);
          Sort s(new BoolectorFunctionSort(btor, btor_fun_sort, sorts, sort));
          return s;
        }
      else
        {
          std::string msg("Can't create Kind ");
          msg += to_string(k);
          msg += " with a vector of sorts and a sort";
          throw IncorrectUsageException(msg.c_str());
        }
    }
    Op construct_op(PrimOp prim_op, unsigned int idx) const override
    {
      IndexedOp io = std::make_shared<BoolectorSingleIndexOp>(prim_op, idx);
      Op op = io;
      return op;
    }
    Op construct_op(PrimOp prim_op, unsigned int idx0, unsigned int idx1) const override
    {
      if (prim_op != BVEXTRACT)
      {
        std::string msg("Can't construct op from ");
        msg += to_string(prim_op);
        msg += " with two integer indices.";
        throw IncorrectUsageException(msg.c_str());
      }
      IndexedOp io = std::make_shared<BoolectorExtractOp>(prim_op, idx0, idx1);
      Op op(io);
      return op;
    }
    // TODO: Add apply_op for unary, binary and ternary ops
    Term apply_op(PrimOp op, Term t) const override
    {
      try
      {
        std::shared_ptr<BoolectorTerm> bt = std::static_pointer_cast<BoolectorTerm>(t);
        BoolectorNode * result = unary_ops.at(op)(btor, bt->node);
        Term term(new BoolectorTerm(btor,
                                    result,
                                    std::vector<Term>{t},
                                    op));
        return term;
      }
      catch(std::out_of_range o)
      {
        std::string msg("Can't apply ");
        msg += to_string(op);
        msg += " to a single term.";
        throw IncorrectUsageException(msg.c_str());
      }
    }
    Term apply_op(PrimOp op, Term t0, Term t1) const override
    {
      try
      {
        std::shared_ptr<BoolectorTerm> bt0 = std::static_pointer_cast<BoolectorTerm>(t0);
        std::shared_ptr<BoolectorTerm> bt1 = std::static_pointer_cast<BoolectorTerm>(t1);
        BoolectorNode * result = binary_ops.at(op)(btor, bt0->node, bt1->node);
        Term term(new BoolectorTerm(btor,
                                    result,
                                    std::vector<Term>{t0, t1},
                                    op));
        return term;
      }
      catch(std::out_of_range o)
      {
        std::string msg("Can't apply ");
        msg += to_string(op);
        msg += " to a single term.";
          throw IncorrectUsageException(msg.c_str());
      }
    }
    Term apply_op(PrimOp op, Term t0, Term t1, Term t2) const override
    {
      try
        {
          std::shared_ptr<BoolectorTerm> bt0 = std::static_pointer_cast<BoolectorTerm>(t0);
          std::shared_ptr<BoolectorTerm> bt1 = std::static_pointer_cast<BoolectorTerm>(t1);
          std::shared_ptr<BoolectorTerm> bt2 = std::static_pointer_cast<BoolectorTerm>(t2);
          BoolectorNode * result = ternary_ops.at(op)(btor, bt0->node, bt1->node, bt2->node);
          Term term(new BoolectorTerm(btor,
                                      result,
                                      std::vector<Term>{t0, t1, t2},
                                      op));
          return term;
        }
      catch(std::out_of_range o)
        {
          std::string msg("Can't apply ");
          msg += to_string(op);
          msg += " to a single term.";
          throw IncorrectUsageException(msg.c_str());
        }
    }
    Term apply_op(PrimOp op, std::vector<Term> terms) const override
    {
      unsigned int size = terms.size();
      // binary ops are most common, check this first
      if (size == 2)
      {
        return apply_op(op, terms[0], terms[1]);
      }
      else if (size == 1)
      {
        return apply_op(op, terms[0]);
      }
      else if (size == 3)
      {
        return apply_op(op, terms[0], terms[1], terms[2]);
      }
      else
      {
        std::string msg("There's no primitive op of arity ");
        msg += std::to_string(size);
        msg += ".";
        throw IncorrectUsageException(msg.c_str());
      }
    }
    Term apply_op(Op op, Term t) const override
    {
      if (std::holds_alternative<PrimOp>(op))
      {
        return apply_op(std::get<PrimOp>(op), t);
      }
      else if (std::holds_alternative<IndexedOp>(op))
      {
        std::shared_ptr<BoolectorIndexedOp> btor_io = std::static_pointer_cast<BoolectorIndexedOp>(std::get<IndexedOp>(op));
        if (btor_io->is_extract_op())
        {
          std::shared_ptr<BoolectorTerm> bt = std::static_pointer_cast<BoolectorTerm>(t);
          BoolectorNode * slice = boolector_slice(btor, bt->node, btor_io->get_upper(), btor_io->get_lower());
          Term term(new BoolectorTerm(btor,
                                      slice,
                                      std::vector<Term>{t},
                                      BVEXTRACT));
          return term;
        }
        else
        {
          // TODO: apply different indexed operations (repeat, zero_extend and sign_extend)
          throw NotImplementedException("Not implemented yet.");
        }
      }
      else
      {
        // rely on the function application in the vector implementation
        return apply_op(op, std::vector<Term>{t});
      }
    }
    Term apply_op(Op op, Term t0, Term t1) const override
    {
      if (std::holds_alternative<PrimOp>(op))
      {
        return apply_op(std::get<PrimOp>(op), t0, t1);
      }
      else if (std::holds_alternative<IndexedOp>(op))
      {
        throw IncorrectUsageException("No indexed operators that take two arguments");
      }
      else
      {
        // rely on the function application in the vector implementation
        return apply_op(op, std::vector<Term>{t0, t1});
      }
    }
    Term apply_op(Op op, Term t0, Term t1, Term t2) const override
    {
      if (std::holds_alternative<PrimOp>(op))
      {
        return apply_op(std::get<PrimOp>(op), t0, t1, t2);
      }
      else if (std::holds_alternative<IndexedOp>(op))
      {
        throw IncorrectUsageException("No indexed operators that take three arguments");
      }
      else
      {
        // rely on the function application in the vector implementation
        return apply_op(op, std::vector<Term>{t0, t1, t2});
      }
    }
    Term apply_op(Op op, std::vector<Term> terms) const override
    {
      unsigned int size = terms.size();
      // Optimization: translate Op to PrimOp as early as possible to prevent unpacking it multipe times
      if (std::holds_alternative<PrimOp>(op))
      {
        return apply_op(std::get<PrimOp>(op), terms);
      }
      else if (size == 1)
      {
        return apply_op(op, terms[0]);
      }
      else if (size == 2)
      {
        return apply_op(op, terms[0], terms[1]);
      }
      else if (size == 3)
      {
        return apply_op(op, terms[0], terms[1], terms[2]);
      }
      else if (std::holds_alternative<Function>(op))
      {
        std::shared_ptr<BoolectorFunction> bf = std::static_pointer_cast<BoolectorFunction>(std::get<Function>(op));
        std::vector<BoolectorNode*> args(size);
        std::shared_ptr<BoolectorTerm> bt;
        for (auto t : terms)
        {
          bt = std::static_pointer_cast<BoolectorTerm>(t);
          args.push_back(bt->node);
        }
        BoolectorNode* result = boolector_apply(btor, &args[0], size, bf->node);
        Term term(new BoolectorTerm(btor,
                                    result,
                                    terms,
                                    op));
      }
      else
      {
        // TODO: make this clearer -- might need to_string for generic op
        std::string msg("Can't find any matching ops to apply to ");
        msg += std::to_string(size);
        msg += " terms.";
        throw IncorrectUsageException(msg.c_str());
      }
    }
  protected:
    Btor * btor;
  };
}

#endif
