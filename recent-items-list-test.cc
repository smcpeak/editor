// recent-items-list-test.cc
// Tests for `recent-items-list` module.

#include "unit-tests.h"                // decl for my entry point
#include "recent-items-list.h"         // module under test

#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ


OPEN_ANONYMOUS_NAMESPACE


void test_basics()
{
  RecentItemsList<int> r;
  r.selfCheck();
  xassert(r.empty());
  xassert(r.getListC() == std::list<int>{});
  xassert(r.getRecentOther(1) == 1);
  xassert(!r.firstOpt().has_value());

  r.add(3);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == std::list<int>{3});
  xassert(r.getRecentOther(1) == 3);
  xassert(r.getRecentOther(3) == 3);
  xassert(r.firstOpt().value() == 3);

  r.add(5);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{5, 3}));
  xassert(r.getRecentOther(1) == 5);
  xassert(r.getRecentOther(3) == 5);
  xassert(r.getRecentOther(5) == 3);
  xassert(r.firstOpt().value() == 5);

  r.add(4);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{4, 5, 3}));
  xassert(r.getRecentOther(1) == 4);
  xassert(r.getRecentOther(4) == 5);
  xassert(r.getRecentOther(5) == 4);
  xassert(r.getRecentOther(3) == 4);
  xassert(r.firstOpt().value() == 4);

  r.add(3);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{3, 4, 5}));
  xassert(r.getRecentOther(1) == 3);
  xassert(r.getRecentOther(3) == 4);
  xassert(r.getRecentOther(4) == 3);
  xassert(r.getRecentOther(5) == 3);
  xassert(r.firstOpt().value() == 3);

  r.add(4);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{4, 3, 5}));
  xassert(r.getRecentOther(1) == 4);
  xassert(r.getRecentOther(4) == 3);
  xassert(r.getRecentOther(3) == 4);
  xassert(r.getRecentOther(5) == 4);
  xassert(r.firstOpt().value() == 4);

  r.remove(4);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{3, 5}));
  xassert(r.getRecentOther(1) == 3);
  xassert(r.getRecentOther(3) == 5);
  xassert(r.getRecentOther(5) == 3);
  xassert(r.getRecentOther(4) == 3);
  xassert(r.firstOpt().value() == 3);

  r.remove(44);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{3, 5}));
  xassert(r.getRecentOther(1) == 3);
  xassert(r.getRecentOther(3) == 5);
  xassert(r.getRecentOther(5) == 3);
  xassert(r.getRecentOther(4) == 3);
  xassert(r.firstOpt().value() == 3);

  r.remove(5);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getListC() == (std::list<int>{3}));
  xassert(r.getRecentOther(1) == 3);
  xassert(r.getRecentOther(3) == 3);
  xassert(r.getRecentOther(5) == 3);
  xassert(r.getRecentOther(4) == 3);
  xassert(r.firstOpt().value() == 3);

  r.remove(3);
  r.selfCheck();
  xassert(r.empty());
  xassert(r.getListC() == (std::list<int>{}));
  xassert(r.getRecentOther(1) == 1);
  xassert(r.getRecentOther(3) == 3);
  xassert(r.getRecentOther(4) == 4);
  xassert(r.getRecentOther(5) == 5);
  xassert(!r.firstOpt().has_value());

  // This clear is a no-op.
  r.clear();
  r.selfCheck();
  xassert(r.empty());
  xassert(!r.firstOpt().has_value());
}


// Just some reference-counted data.
class Integer : public SerfRefCount {
public:      // data
  int m_x;

public:      // methods
  Integer(int x) : m_x(x) {}
};


// Make sure that a list of `RCSerf` works.
void test_Integer()
{
  RecentItemsList<RCSerf<Integer>> r;
  r.selfCheck();
  xassert(r.empty());

  Integer i1{1};
  Integer i2{2};
  Integer i3{3};

  r.add(&i1);
  r.selfCheck();
  xassert(!r.empty());
  xassert(r.getRecentOther(&i1) == &i1);

  r.add(&i2);
  r.selfCheck();
  xassert(r.getRecentOther(&i1) == &i2);
  xassert(r.getRecentOther(&i2) == &i1);

  r.add(&i3);
  r.selfCheck();
  xassert(r.getRecentOther(&i1) == &i3);
  xassert(r.getRecentOther(&i2) == &i3);
  xassert(r.getRecentOther(&i3) == &i2);

  r.remove(&i2);
  r.selfCheck();
  xassert(r.getRecentOther(&i1) == &i3);
  xassert(r.getRecentOther(&i2) == &i3);
  xassert(r.getRecentOther(&i3) == &i1);

  // This clear is necessary to avoid failing the assertion about not
  // dangling an `RCSerf` pointer.
  r.clear();
  r.selfCheck();
  xassert(r.empty());
}


CLOSE_ANONYMOUS_NAMESPACE


// Called by unit-tests.cc.
void test_recent_items_list(CmdlineArgsSpan args)
{
  test_basics();
  test_Integer();
}


// EOF
