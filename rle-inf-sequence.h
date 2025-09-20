// rle-inf-sequence.h
// `RLEInfiniteSequence`, a run-length-encoded infinite sequence.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RLE_INF_SEQUENCE_H
#define EDITOR_RLE_INF_SEQUENCE_H

#include "smbase/chained-cond.h"       // smbase::cc::z_le_le
#include "smbase/sm-macros.h"          // IMEMBFP, NOTEQUAL_OPERATOR, smbase_loopi, EMEMB
#include "smbase/xassert.h"            // xassert[Precondition]

#include <algorithm>                   // std::min
#include <sstream>                     // std::ostringstream
#include <string>                      // std::string
#include <utility>                     // std::swap
#include <vector>                      // std::vector


// A run-length-encoded infinite sequence of `T`.
template <typename T>
class RLEInfiniteSequence {
private:     // types
  // Contiguous run of elements with the same value.
  class Run {
  public:      // data
    // The value for all elements in the run.
    T m_value;

    // Number of elements in the run.
    //
    // Invariant: m_length >= 0
    int m_length;

  public:      // methods
    Run(T const &value, int length)
    :
      IMEMBFP(value),
      IMEMBFP(length)
    {
      selfCheck();
    }

    // Assert invariants.
    void selfCheck() const
    {
      xassert(m_length >= 0);
    }

    bool operator==(Run const &obj) const
    {
      return EMEMB(m_value) &&
             EMEMB(m_length);
    }
    NOTEQUAL_OPERATOR(Run)
  };

public:      // types
  class Iter {
  private:     // data
    // Sequence we are iterating over.
    RLEInfiniteSequence const &m_seq;

    // Index into `m_seq.m_runs` that we will read once we have finished
    // with the data loaded into `m_run`.  Can equal `m_seq.numRuns()`,
    // meaning we have no more runs to read.
    //
    // Invariant: 0 <= m_index <= m_seq.numRuns()
    int m_index;

    // Current run of data being processed.
    Run m_run;

  public:      // methods
    explicit Iter(RLEInfiniteSequence const &seq)
    :
      IMEMBFP(seq),
      m_index(0),
      m_run(seq.m_tailValue, 0)
    {
      if (!atEnd()) {
        nextRun();
      }
    }

    // Assert invariants.
    void selfCheck() const
    {
      xassert(smbase::cc::z_le_le(m_index, m_seq.numRuns()));
    }

    // Value for the current run.
    T const &value() const
    {
      return m_run.m_value;
    }

    // Remaining elements in the current run.
    //
    // If `atEnd()`, then this returns 0, but the true length is
    // infinite.
    int runLength() const
    {
      return m_run.m_length;
    }

    // Return the current run length of `*this` or `rhsIter`, whichever
    // is smaller.
    template <typename T2>
    int minRunLengthRelTo(
      typename RLEInfiniteSequence<T2>::Iter const &rhsIter) const
    {
      if (atEnd()) {
        // If they are both at the end, this will return 0, but this
        // method should not be called in that case.
        return rhsIter.runLength();
      }
      else if (rhsIter.atEnd()) {
        return this->runLength();
      }
      else {
        return std::min(this->runLength(), rhsIter.runLength());
      }
    }

    // Advance the iterator by `count` elements.
    //
    // Requires: count >= 0
    void advance(int count)
    {
      xassertPrecondition(count >= 0);

      while (count > 0 && !atEnd()) {
        if (runLength() <= count) {
          count -= runLength();
          nextRun();
        }
        else {
          m_run.m_length -= count;
          count = 0;
        }
      }

      selfCheck();
    }

    // Move to the next run.
    //
    // Requires: !atEnd()
    void nextRun()
    {
      xassertPrecondition(!atEnd());

      if (m_index < m_seq.numRuns()) {
        m_run = m_seq.m_runs.at(m_index);
        ++m_index;
      }
      else {
        m_run.m_value = m_seq.m_tailValue;
        m_run.m_length = 0;
      }
    }

    // True if we have reached the infinite tail.
    bool atEnd() const
    {
      return m_index == m_seq.numRuns() &&
             runLength() == 0;
    }
  };

private:     // data
  // Finite portion of the sequence.
  std::vector<Run> m_runs;

  // Value for the infinite tail.
  T m_tailValue;

private:     // methods
  int numRuns() const
  {
    return static_cast<int>(m_runs.size());
  }

  // True if there is a final run, and its value is `value`.
  bool finalRunHasValue(T const &value) const
  {
    return !m_runs.empty() &&
           m_runs.back().m_value == value;
  }

public:      // methods
  ~RLEInfiniteSequence()
  {}

  // Sequence of all `tailValue`.
  explicit RLEInfiniteSequence(T const &tailValue = T())
  :
    m_runs(),
    IMEMBFP(tailValue)
  {}

  RLEInfiniteSequence(RLEInfiniteSequence const &obj) = default;
  RLEInfiniteSequence(RLEInfiniteSequence &&obj) = default;

  RLEInfiniteSequence &operator=(RLEInfiniteSequence const &obj) = default;
  RLEInfiniteSequence &operator=(RLEInfiniteSequence &&obj) = default;

  // Assert invariants.
  void selfCheck() const
  {
    for (Run const &run : m_runs) {
      run.selfCheck();
    }
  }

  void swapWith(RLEInfiniteSequence &obj)
  {
    using std::swap;
    swap(m_runs, obj.m_runs);
    swap(m_tailValue, obj.m_tailValue);
  }

  // Reset sequence to all `tailValue`.
  void clear(T const &tailValue)
  {
    m_runs.clear();
    m_tailValue = tailValue;
  }

  // Add additional elements after the last finite run but before the
  // infinite tail.
  void append(T const &value, int length)
  {
    if (length > 0) {
      if (finalRunHasValue(value)) {
        // Extend the final run.
        m_runs.back().m_length += length;
      }
      else {
        // Add a new run.
        m_runs.push_back(Run{value, length});
      }
    }
  }

  // Set the tail value, and remove the final run if it has the same
  // value.  This should be done after appending all of the runs of the
  // finite portion of the sequence.
  void setTailValue(T const &tail)
  {
    m_tailValue = tail;

    if (finalRunHasValue(tail)) {
      // Remove the final run as redundant.
      m_runs.pop_back();

      // We should not need to look any further back since it should
      // not be possible to get contiguous runs with the same value.
    }
  }

  bool operator==(RLEInfiniteSequence const &obj) const
  {
    return EMEMB(m_runs) &&
           EMEMB(m_tailValue);
  }
  NOTEQUAL_OPERATOR(RLEInfiniteSequence)

  // Get the value at a particular position.
  //
  // The returned reference is invaliated by any non-const method.
  //
  // Requires: index >= 0
  T const &at(int index) const
  {
    xassertPrecondition(index >= 0);

    for (Run const &run : m_runs) {
      if (run.m_length > index) {
        return run.m_value;
      }
      index -= run.m_length;
    }

    return m_tailValue;
  }

  T const &tailValue() const
  {
    return m_tailValue;
  }

  // Return a string like "[V1,L1][V2,L2][Vtail" where "Vi" are the
  // stringified values and "Li" the run lengths.
  std::string asString() const
  {
    std::ostringstream oss;

    for (Run const &run : m_runs) {
      oss << "[" << run.m_value << "," << run.m_length << "]";
    }

    oss << "[" << m_tailValue;

    return oss.str();
  }

  // Return a string like "V1V1V1V2V2Vtail...".
  std::string asUnaryString() const
  {
    std::ostringstream oss;

    for (Run const &run : m_runs) {
      smbase_loopi(run.m_length) {
        oss << run.m_value;
      }
    }

    oss << m_tailValue << "...";

    return oss.str();
  }
};


// Combine the elements of `lhs` and `rhs` pointwise with
// `combineElements`.
template <typename RESULT,
          typename LHS_ELT,
          typename RHS_ELT,
          typename Callable>
RLEInfiniteSequence<RESULT> combineSequences(
  RLEInfiniteSequence<LHS_ELT> const &lhs,
  RLEInfiniteSequence<RHS_ELT> const &rhs,
  Callable &&combineElements)
{
  RLEInfiniteSequence<RESULT> dest;

  typename RLEInfiniteSequence<LHS_ELT>::Iter lhsIter(lhs);
  typename RLEInfiniteSequence<RHS_ELT>::Iter rhsIter(rhs);

  while (!lhsIter.atEnd() || !rhsIter.atEnd()) {
    int len = lhsIter.template minRunLengthRelTo<RHS_ELT>(rhsIter);

    dest.append(combineElements(lhsIter.value(), rhsIter.value()), len);

    lhsIter.advance(len);
    rhsIter.advance(len);
  }

  xassert(lhsIter.atEnd() && rhsIter.atEnd());

  // Combine the tail values.
  dest.setTailValue(combineElements(lhsIter.value(), rhsIter.value()));

  return dest;
}


#endif // EDITOR_RLE_INF_SEQUENCE_H
