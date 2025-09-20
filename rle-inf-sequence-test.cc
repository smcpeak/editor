// rle-inf-sequence-test.cc
// Tests for `rle-inf-sequence` module.

#include "unit-tests.h"                // decl for my entry point
#include "rle-inf-sequence.h"          // module under test

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, smbase_loopi
#include "smbase/sm-test.h"            // EXPECT_{EQ,TRUE,FALSE}
#include "smbase/stringb.h"            // stringb

#include <optional>                    // std::optional

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


template <typename T>
void checkIteratorVsAt(RLEInfiniteSequence<T> const &seq)
{
  int index = 0;

  typename RLEInfiniteSequence<T>::Iter it(seq);
  it.selfCheck();

  for (; !it.atEnd(); it.nextRun()) {
    it.selfCheck();

    smbase_loopi(it.runLength()) {
      xassert(seq.at(index + i) == it.value());
    }

    index += it.runLength();
  }

  it.selfCheck();
  EXPECT_TRUE(it.atEnd());
  EXPECT_EQ(it.runLength(), 0);
  xassert(it.value() == seq.tailValue());
  xassert(seq.at(index) == seq.tailValue());
}


template <typename T>
void checkSeq(RLEInfiniteSequence<T> const &seq)
{
  seq.selfCheck();

  checkIteratorVsAt(seq);

  seq.selfCheck();
}


void test_basics()
{
  // Sequence of all "9".
  RLEInfiniteSequence<int> seq(9);
  checkSeq(seq);

  EXPECT_EQ(seq.at(0), 9);
  EXPECT_EQ(seq.at(1), 9);
  EXPECT_EQ(seq.at(2), 9);
  EXPECT_EQ(seq.asString(), "[9");
  EXPECT_EQ(seq.asUnaryString(), "9...");

  {
    RLEInfiniteSequence<int>::Iter it(seq);
    it.selfCheck();

    EXPECT_EQ(it.value(), 9);
    EXPECT_EQ(it.runLength(), 0);
    EXPECT_TRUE(it.atEnd());

    // Advancing when at the end does not change anything.
    it.advance(1);
    EXPECT_EQ(it.value(), 9);
    EXPECT_EQ(it.runLength(), 0);
    EXPECT_TRUE(it.atEnd());
  }

  // Sequence of 3 "1", then all "9".
  seq.append(1, 3);
  checkSeq(seq);

  EXPECT_EQ(seq.at(0), 1);
  EXPECT_EQ(seq.at(1), 1);
  EXPECT_EQ(seq.at(2), 1);
  EXPECT_EQ(seq.at(3), 9);
  EXPECT_EQ(seq.at(4), 9);
  EXPECT_EQ(seq.asString(), "[1,3][9");
  EXPECT_EQ(seq.asUnaryString(), "1119...");

  {
    RLEInfiniteSequence<int>::Iter it(seq);
    it.selfCheck();

    EXPECT_EQ(it.value(), 1);
    EXPECT_EQ(it.runLength(), 3);
    EXPECT_FALSE(it.atEnd());

    // Advance partway into this run.
    it.advance(1);

    EXPECT_EQ(it.value(), 1);
    EXPECT_EQ(it.runLength(), 2);
    EXPECT_FALSE(it.atEnd());

    // Advance to the next run.
    it.nextRun();

    EXPECT_EQ(it.value(), 9);
    EXPECT_EQ(it.runLength(), 0);
    EXPECT_TRUE(it.atEnd());
  }
}


// Append behavior, including run merging.
void test_append_and_merge()
{
  RLEInfiniteSequence<char> seq('X');
  checkSeq(seq);

  seq.append('A', 2);
  seq.append('A', 3);   // should merge with previous run
  seq.append('B', 1);
  seq.append('Y', 0);   // length=0: no-op
  checkSeq(seq);

  EXPECT_EQ(seq.asString(), "[A,5][B,1][X");
  EXPECT_EQ(seq.asUnaryString(), "AAAAABX...");
  EXPECT_EQ(seq.at(0), 'A');
  EXPECT_EQ(seq.at(4), 'A');
  EXPECT_EQ(seq.at(5), 'B');
  EXPECT_EQ(seq.at(6), 'X');
  EXPECT_EQ(seq.at(10), 'X');
}


// Equality and clear().
void test_equality_and_clear()
{
  RLEInfiniteSequence<int> a(0);
  a.append(1, 2);
  a.append(2, 3);
  checkSeq(a);

  RLEInfiniteSequence<int> b(0);
  b.append(1, 2);
  b.append(2, 3);
  checkSeq(b);

  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);

  b.append(3, 1);
  checkSeq(b);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);

  // Clear resets to all tail value.
  a.clear(7);
  checkSeq(a);
  EXPECT_EQ(a.asString(), "[7");
  EXPECT_EQ(a.at(0), 7);
  EXPECT_EQ(a.at(100), 7);
}


// Iterator advance across multiple runs.
void test_iterator_advance()
{
  RLEInfiniteSequence<int> seq(9);
  seq.append(1, 2);
  seq.append(2, 2);
  checkSeq(seq);

  RLEInfiniteSequence<int>::Iter it(seq);

  EXPECT_EQ(it.value(), 1);
  EXPECT_EQ(it.runLength(), 2);

  it.advance(2);   // consume first run
  EXPECT_EQ(it.value(), 2);
  EXPECT_EQ(it.runLength(), 2);

  it.advance(1);
  EXPECT_EQ(it.value(), 2);
  EXPECT_EQ(it.runLength(), 1);

  it.advance(1);   // move to tail
  EXPECT_EQ(it.value(), 9);
  EXPECT_EQ(it.runLength(), 0);
  EXPECT_TRUE(it.atEnd());
}


// combineSequences with different result types.
void test_combine_sequences()
{
  RLEInfiniteSequence<int> lhs(0);
  lhs.append(1, 2);
  lhs.append(2, 1);
  checkSeq(lhs);
  EXPECT_EQ(lhs.asString(), "[1,2][2,1][0");

  RLEInfiniteSequence<int> rhs(5);
  rhs.append(3, 1);
  rhs.append(4, 2);
  checkSeq(rhs);
  EXPECT_EQ(rhs.asString(), "[3,1][4,2][5");

  {
    // Combine by addition.
    auto add = [](int a, int b) -> int { return a + b; };

    {
      auto sum = combineSequences<int>(lhs, rhs, add);

      EXPECT_EQ(sum.asString(), "[4,1][5,1][6,1][5");
      EXPECT_EQ(sum.at(0), 4);  // 1+3
      EXPECT_EQ(sum.at(1), 5);  // 1+4
      EXPECT_EQ(sum.at(2), 6);  // 2+4
      EXPECT_EQ(sum.at(3), 5);  // 0+5 tail
      EXPECT_EQ(sum.at(10), 5);

      checkSeq(sum);
    }

    RLEInfiniteSequence<int> zero;
    EXPECT_EQ(combineSequences<int>(lhs, zero, add).asString(),
     "[1,2][2,1][0");
    EXPECT_EQ(combineSequences<int>(zero, rhs, add).asString(),
     "[3,1][4,2][5");

    RLEInfiniteSequence<int> one(1);
    EXPECT_EQ(combineSequences<int>(lhs, one, add).asString(),
     "[2,2][3,1][1");
    EXPECT_EQ(combineSequences<int>(one, rhs, add).asString(),
     "[4,1][5,2][6");
  }

  {
    // Combine by equality (bool result).
    auto eq = combineSequences<bool>(lhs, rhs,
      [](int a, int b) -> bool { return a == b; });

    EXPECT_EQ(eq.asString(), "[0");
    EXPECT_FALSE(eq.at(0));
    EXPECT_FALSE(eq.at(1));
    EXPECT_FALSE(eq.at(2));
    EXPECT_FALSE(eq.at(100));

    checkSeq(eq);
  }

  {
    // Combine by string concatenation.
    auto cats = combineSequences<std::string>(lhs, rhs,
      [](int a, int b) -> std::string {
        return stringb(a << b);
      });

    EXPECT_EQ(cats.asString(), "[13,1][14,1][24,1][05");

    checkSeq(cats);
  }

  lhs.swapWith(rhs);
  checkSeq(lhs);
  checkSeq(rhs);
  EXPECT_EQ(lhs.asString(), "[3,1][4,2][5");
  EXPECT_EQ(rhs.asString(), "[1,2][2,1][0");
}


// Different argument types.
void test_combineSequences_hetero()
{
  RLEInfiniteSequence<int> base(9);
  base.append(3,3);
  base.append(2,2);
  base.append(1,1);
  checkSeq(base);

  RLEInfiniteSequence<std::optional<int>> overrides;
  overrides.append(std::nullopt, 4);
  overrides.append(6, 3);
  checkSeq(overrides);

  auto combine =
    [](int b, std::optional<int> o) -> int {
      if (o) {
        return *o;
      }
      else {
        return b;
      }
    };

  RLEInfiniteSequence<int> result =
    combineSequences<int>(base, overrides, combine);
  EXPECT_EQ(result.asString(), "[3,3][2,1][6,3][9");
  EXPECT_EQ(result.asUnaryString(), "33326669...");
  checkSeq(result);

  overrides.clear(7);
  result = combineSequences<int>(base, overrides, combine);
  EXPECT_EQ(result.asString(), "[7");
  EXPECT_EQ(result.asUnaryString(), "7...");
  checkSeq(result);

  overrides.append(std::nullopt, 4);
  result = combineSequences<int>(base, overrides, combine);
  EXPECT_EQ(result.asString(), "[3,3][2,1][7");
  EXPECT_EQ(result.asUnaryString(), "33327...");
  checkSeq(result);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_rle_inf_sequence(CmdlineArgsSpan args)
{
  test_basics();
  test_append_and_merge();
  test_equality_and_clear();
  test_iterator_advance();
  test_combine_sequences();
  test_combineSequences_hetero();
}


// EOF
