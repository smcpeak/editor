// line-index.h
// `LineIndex`, to represent a 0-based index into an array of lines.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_INDEX_H
#define EDITOR_LINE_INDEX_H

#include "line-index-fwd.h"            // fwds for this module

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/xassert.h"            // xassertPrecondition

#include <iosfwd>                      // std::ostream


/* 0-based index into an array of lines, generally used in internal data
   structures.

   This type exists to prevent confusion with `LineNumber`, the 1-based
   variation generally used in user interfaces.

   TODO: This class is an example of a family of classes that works like
   an integer but is not implicitly convertible to other integers and
   has a constraint on the value.  `smbase::DistinctNumber` does the
   former but not the latter.  I'd like to find a way to generalize this
   fully so this can just be a template specialization.
*/
class LineIndex final {
private:     // data
  // The index.  Non-negative.
  int m_value;

public:      // methods
  // Requires: value >= 0
  explicit LineIndex(int value = 0);

  LineIndex(LineIndex const &obj);
  LineIndex &operator=(LineIndex const &obj);

  // Assert invariants.
  void selfCheck() const;

  // Ensures: return >= 0
  int get() const
    { return m_value; }

  // I'm using this as a sort of marker, for places where for the moment
  // I need `get()`, but my intention is to change the interface of the
  // thing receiving the `int` to the `get()` call is not needed.
  int getForNow() const
    { return get(); }

  // Compare in the usual order for integers.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineIndex);

  // Returns a GDV integer.
  operator gdv::GDValue() const;

  // Expects an integer, throws `XGDValueError` if it is negative or
  // too large to represent.
  explicit LineIndex(gdv::GDValueParser const &p);

  // Write using `os << m_value`.
  void write(std::ostream &os) const;

  friend std::ostream &operator<<(std::ostream &os,
                                  LineIndex const &obj)
  {
    obj.write(os);
    return os;
  }
};


#endif // EDITOR_LINE_INDEX_H
