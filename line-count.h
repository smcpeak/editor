// line-count.h
// `LineCount`, a non-negative `LineDifference`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_COUNT_H
#define EDITOR_LINE_COUNT_H

#include "line-count-fwd.h"            // fwds for this module

#include "line-difference.h"           // LineDifference

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS, DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/sm-macros.h"          // IMEMBFP, CMEMB, DMEMB
#include "smbase/std-string-fwd.h"     // std::string

#include <iosfwd>                      // std::ostream


// A non-negative `LineDifference`.
//
// TODO: Factor the commonality with `LineDifference`.
class LineCount final {
private:     // data
  // The value.  Non-negative.
  int m_value;

public:      // methods
  // Requires: value >= 0
  explicit LineCount(int value = 0)
    : m_value(value)
  {
    selfCheck();
  }

  // Requires: value >= 0
  explicit LineCount(LineDifference value)
    : m_value(value.get())
  {
    selfCheck();
  }

  LineCount(LineCount const &obj)
    : DMEMB(m_value)
  {
    selfCheck();
  }

  LineCount &operator=(LineCount const &obj)
  {
    if (this != &obj) {
      CMEMB(m_value);
      selfCheck();
    }
    return *this;
  }

  // Assert invariants.
  void selfCheck() const;

  int get() const
    { return m_value; }

  // Requires: value >= 0
  void set(int value)
    { m_value = value; selfCheck(); }
  void set(LineDifference value)
    { m_value = value.get(); selfCheck(); }

  // Since a `LineCount` is logically just a restricted
  // `LineDifference`, allow it to be implicitly converted.
  operator LineDifference() const
    { return LineDifference(m_value); }

  // --------------------------- Unary tests ---------------------------
  bool isZero() const
    { return m_value == 0; }
  bool isPositive() const
    { return m_value > 0; }

  explicit operator bool() const
    { return isPositive(); }

  // -------------------------- Binary tests ---------------------------
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineCount);
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineCount, int);
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineCount, LineDifference);

  // ---------------------------- Addition -----------------------------
  LineCount operator+() const;
  LineCount operator+(LineCount delta) const;

  // Requires: delta >= 0
  LineCount operator+(int delta) const;

  LineCount &operator+=(LineCount delta);

  // Requires: delta >= 0
  LineCount &operator+=(int delta);

  LineCount &operator++();
  LineCount operator++(int);

  // ---------------------- Subtraction/inversion ----------------------
  // Inversion widens to the difference type.
  LineDifference operator-() const;
  LineDifference operator-(LineCount delta) const;
  LineDifference operator-(int delta) const;

  // Requires: result is non-negative.
  LineCount &operator-=(LineCount delta);

  // Requires: isPositive()
  LineCount &operator--();
  LineCount operator--(int);

  // Requires: isPositive()
  LineCount nzpred() const;

  // -------------------------- Serialization --------------------------
  // Returns a GDV integer.
  operator gdv::GDValue() const;

  // Expects an integer, throws `XGDValueError` if it is negative or
  // too large to represent.
  explicit LineCount(gdv::GDValueParser const &p);

  // Write using `os << m_value`.
  void write(std::ostream &os) const;

  friend std::ostream &operator<<(std::ostream &os,
                                  LineCount const &obj)
  {
    obj.write(os);
    return os;
  }
};


#endif // EDITOR_LINE_COUNT_H
