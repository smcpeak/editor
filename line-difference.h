// line-difference.h
// `LineDifference`, to represent a difference between two line indices
// or line numbers.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_DIFFERENCE_H
#define EDITOR_LINE_DIFFERENCE_H

#include "line-difference-fwd.h"       // fwds for this module

#include "smbase/compare-util-iface.h" // DECLARE_COMPARETO_AND_DEFINE_RELATIONALS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/sm-macros.h"          // IMEMBFP, CMEMB, DMEMB
#include "smbase/std-string-fwd.h"     // std::string

#include <iosfwd>                      // std::ostream


// --------------------- BEGIN: To copy to smbase ----------------------
// Define one operator that compares this `Class` to `Other` in either
// direction.
#define DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, op) \
  [[maybe_unused]]                                                       \
  friend bool operator op (Class const &a, Other const &b)               \
    { return compare(a,b) op 0; }                                        \
  [[maybe_unused]]                                                       \
  friend bool operator op (Other const &a, Class const &b)               \
    { return compare(a,b) op 0; }


// Declare a set of friend comparison-to-other operators, *excluding*
// the equality operators, assuming that a 'compare' function exists.
#define DEFINE_FRIEND_NON_EQUALITY_RELATIONAL_TO_OTHER_OPERATORS(Class, Other) \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, < )             \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, <=)             \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, > )             \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, >=)


// Declare a set of friend comparison-to-other operators, assuming that
// a 'compare' function exists.
#define DEFINE_FRIEND_RELATIONAL_TO_OTHER_OPERATORS(Class, Other)        \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, ==)       \
  DEFINE_ONE_FRIEND_RELATIONAL_TO_OTHER_OPERATOR(Class, Other, !=)       \
  DEFINE_FRIEND_NON_EQUALITY_RELATIONAL_TO_OTHER_OPERATORS(Class, Other)


/* Declare a `compareTo` method, to compare to an `Other` type, that
   must be implemented elsewhere.  Then, define friend `compare` methods
   terms of it, and friend relational operators in terms of that.
*/
#define DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(Class, Other) \
  int compareTo(Other const &b) const;                                  \
  [[maybe_unused]]                                                      \
  friend int compare(Class const &a, Other const &b)                    \
    { return a.compareTo(b); }                                          \
  [[maybe_unused]]                                                      \
  friend int compare(Other const &a, Class const &b)                    \
    { return -(b.compareTo(a)); }                                       \
  DEFINE_FRIEND_RELATIONAL_TO_OTHER_OPERATORS(Class, Other)


// ---------------------- END: To copy to smbase -----------------------


/* This type is an experiment in creating unique integer-like types for
   more fine-grained purposes than I have in the past.  The initial idea
   is to yield this from `LineIndex::operator-` and to allow it to be
   used implicitly with `LineIndex` arithmetic.

   For the moment this is largely copy+paste since I'm not sure what
   parts, or how, I want to factor this with `LineIndex`.
*/
class LineDifference final {
private:     // data
  // The difference value.  Can be negative or positive.
  int m_value;

public:      // methods
  // Since the primary purpose of this class is to be a distinct integer
  // type, conversions into and out must be explicit.
  explicit LineDifference(int value = 0)
    : m_value(value)
  {
    selfCheck();
  }

  LineDifference(LineDifference const &obj)
    : DMEMB(m_value)
  {
    selfCheck();
  }

  LineDifference &operator=(LineDifference const &obj)
  {
    if (this != &obj) {
      CMEMB(m_value);
      selfCheck();
    }
    return *this;
  }

  // Assert invariants.  Although I have nothing to check, and have no
  // plans to add anything, this is becoming a part of my standard
  // interface, so I'll provide it.
  void selfCheck () const {}

  int get() const
    { return m_value; }

  // I think omitting this in `LineIndex` was a mistake.  It is
  // sufficiently explicit.
  void set(int value)
    { m_value = value; }

  // Compare in the usual order for integers.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineDifference);

  // Comparing to ints should be fine, although I don't know whether I
  // will run into problems with ambiguities.
  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS_TO_OTHER(LineDifference, int);

  explicit operator bool() const
    { return m_value != 0; }

  LineDifference &operator++();
  LineDifference operator++(int);

  LineDifference &operator--();
  LineDifference operator--(int);

  LineDifference operator+(LineDifference delta) const;
  LineDifference operator+(int delta) const;

  LineDifference &operator+=(LineDifference delta);
  LineDifference &operator+=(int delta);

  LineDifference operator-(LineDifference delta) const;
  LineDifference operator-(int delta) const;

  LineDifference &operator-=(LineDifference delta);
  LineDifference &operator-=(int delta);

  LineDifference operator+() const;
  LineDifference operator-() const;

  // If `m_value < value`, set it equal to `value`, such that `value`
  // acts as a lower bound to be clamped to.
  void clampLower(int value);

  // Returns a GDV integer.
  operator gdv::GDValue() const;

  // Expects an integer, throws `XGDValueError` if it is negative or
  // too large to represent.
  explicit LineDifference(gdv::GDValueParser const &p);

  // Write using `os << m_value`.
  void write(std::ostream &os) const;

  friend std::ostream &operator<<(std::ostream &os,
                                  LineDifference const &obj)
  {
    obj.write(os);
    return os;
  }
};


// Return the string that `obj.write` writes.
std::string toString(LineDifference const &obj);


#endif // EDITOR_LINE_DIFFERENCE_H
