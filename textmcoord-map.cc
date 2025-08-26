// textmcoord-map.cc
// Code for `textmcoord-map.h`.

// See license.txt for copyright and terms of use.

#include "smbase/gdvalue-optional-fwd.h"         // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-set-fwd.h"              // gdv::toGDValue(std::set)

#include "textmcoord-map.h"                      // this module

#include "td-core.h"                             // TextDocumentCore

#include "smbase/chained-cond.h"                 // smbase::cc::{z_le_lt, etc.}
#include "smbase/compare-util-iface.h"           // RET_IF_COMPARE_MEMBERS
#include "smbase/container-util.h"               // smbase::contains
#include "smbase/gdvalue-optional.h"             // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-set.h"                  // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/map-util.h"                     // mapMoveValueAt
#include "smbase/optional-util.h"                // optAccumulateMax
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/set-util.h"                     // smbase::setInsertUnique
#include "smbase/sm-macros.h"                    // DMEMB, EMEMB, IMEMBFP
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.

#include <optional>                              // std::optional

using namespace gdv;
using namespace smbase;


INIT_TRACE("textmcoord-map");


// ----------------------------- DocEntry ------------------------------
TextMCoordMap::DocEntry::DocEntry(TextMCoordRange range, Value value)
  : m_range(range),
    m_value(value)
{
  selfCheck();
}


void TextMCoordMap::DocEntry::selfCheck() const
{
  xassert(m_range.m_start <= m_range.m_end);
}


TextMCoordMap::DocEntry::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("DocEntry"_sym);

  GDV_WRITE_MEMBER_SYM(m_range);
  GDV_WRITE_MEMBER_SYM(m_value);

  return m;
}


int TextMCoordMap::DocEntry::compareTo(DocEntry const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_range);
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


// ----------------------------- LineEntry -----------------------------
TextMCoordMap::LineEntry::LineEntry(
  std::optional<int> startByteIndex,
  std::optional<int> endByteIndex,
  Value value)
  : m_startByteIndex(startByteIndex),
    m_endByteIndex(endByteIndex),
    m_value(value)
{
  selfCheck();
}


void TextMCoordMap::LineEntry::selfCheck()
{
  if (m_startByteIndex.has_value() && m_endByteIndex.has_value()) {
    xassert(m_startByteIndex.value() <= m_endByteIndex.value());
  }
}


TextMCoordMap::LineEntry::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("LineEntry"_sym);

  GDV_WRITE_MEMBER_SYM(m_startByteIndex);
  GDV_WRITE_MEMBER_SYM(m_endByteIndex);
  GDV_WRITE_MEMBER_SYM(m_value);

  return m;
}


TextMCoordMap::LineEntry::LineEntry(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_SYM(m_startByteIndex),
    GDVP_READ_MEMBER_SYM(m_endByteIndex),
    GDVP_READ_MEMBER_SYM(m_value)
{
  // This is a novel pattern for me: read the incoming data, then run
  // `selfCheck` in a handler that maps any problems to a
  // parsing-related exception type.  This way the client only has to
  // deal with the latter, and the exception carries context related to
  // where we are in the parsing stream.
  //
  // This approach seems promising, although I should probably change
  // the assertion type used for the underlying check from `XAssert` to
  // something that indicates it is related to a violated invariant.
  //
  try {
    selfCheck();
  }
  catch (XBase &x) {
    p.throwError(x.getMessage());
  }
}


int TextMCoordMap::LineEntry::compareTo(LineEntry const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_startByteIndex);
  RET_IF_COMPARE_MEMBERS(m_endByteIndex);
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


// -------------------------- SingleLineSpan ---------------------------
TextMCoordMap::SingleLineSpan::SingleLineSpan(
  int startByteIndex,
  int endByteIndex,
  Value value)
  : m_startByteIndex(startByteIndex),
    m_endByteIndex(endByteIndex),
    m_value(value)
{
  selfCheck();
}


void TextMCoordMap::SingleLineSpan::selfCheck() const
{
  xassert(0 <= m_startByteIndex);
  xassert(     m_startByteIndex <= m_endByteIndex);
}


TextMCoordMap::SingleLineSpan::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("SingleLineSpan"_sym);

  GDV_WRITE_MEMBER_SYM(m_startByteIndex);
  GDV_WRITE_MEMBER_SYM(m_endByteIndex);
  GDV_WRITE_MEMBER_SYM(m_value);

  return m;
}


int TextMCoordMap::SingleLineSpan::compareTo(SingleLineSpan const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_startByteIndex);
  RET_IF_COMPARE_MEMBERS(m_endByteIndex);
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


// ----------------------------- Boundary ------------------------------
TextMCoordMap::Boundary::Boundary(int byteIndex, Value value)
  : m_byteIndex(byteIndex),
    m_value(value)
{
  selfCheck();
}


void TextMCoordMap::Boundary::selfCheck() const
{
  xassert(0 <= m_byteIndex);
}


TextMCoordMap::Boundary::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("Boundary"_sym);

  GDV_WRITE_MEMBER_SYM(m_byteIndex);
  GDV_WRITE_MEMBER_SYM(m_value);

  return m;
}


int TextMCoordMap::Boundary::compareTo(Boundary const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_byteIndex);
  RET_IF_COMPARE_MEMBERS(m_value);
  return 0;
}


// ----------------------------- LineData ------------------------------
TextMCoordMap::LineData::~LineData()
{}


TextMCoordMap::LineData::LineData(LineData const &obj)
  : DMEMB(m_singleLineSpans),
    DMEMB(m_startsHere),
    DMEMB(m_continuesHere),
    DMEMB(m_endsHere)
{
  selfCheck();
}


TextMCoordMap::LineData::LineData()
  : m_singleLineSpans(),
    m_startsHere(),
    m_continuesHere(),
    m_endsHere()
{
  selfCheck();
}


bool TextMCoordMap::LineData::operator==(LineData const &obj) const
{
  return EMEMB(m_singleLineSpans) &&
         EMEMB(m_startsHere) &&
         EMEMB(m_continuesHere) &&
         EMEMB(m_endsHere);
}


void TextMCoordMap::LineData::selfCheck() const
{
  for (SingleLineSpan const &span : m_singleLineSpans) {
    span.selfCheck();
  }

  for (Boundary const &b : m_startsHere) {
    b.selfCheck();
  }

  // There isn't anything to check for `m_continuesHere`.

  for (Boundary const &b : m_endsHere) {
    b.selfCheck();
  }
}


TextMCoordMap::LineData::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("LineData"_sym);

  GDV_WRITE_MEMBER_SYM(m_singleLineSpans);
  GDV_WRITE_MEMBER_SYM(m_startsHere);
  GDV_WRITE_MEMBER_SYM(m_continuesHere);
  GDV_WRITE_MEMBER_SYM(m_endsHere);

  return m;
}


// Accumulates planned changes to a set.  Set elements cannot be
// directly modified.
//
// This is a candidate to move someplace more general.
template <typename K,
          typename C = std::less<K>,
          typename A = std::allocator<K>>
class DelayedSetChanges {
public:      // types
  typedef std::set<K,C,A> Set;

private:     // data
  // Items to remove at the end.
  Set m_toRemove;

  // Items to insert at the end.
  Set m_toInsert;

public:
  ~DelayedSetChanges() = default;
  DelayedSetChanges() = default;

  // Arrange to replace `oldValue` with `newValue` on `execute`.
  void replaceWith(K const &oldValue, K &&newValue)
  {
    m_toRemove.insert(oldValue);
    m_toInsert.insert(std::move(newValue));
  }

  // Perform the accumulated modifications on `s`.  This must be the
  // last thing done with this object since it leaves `m_toInsert` in an
  // indeterminate state.
  void execute(Set &s)
  {
    setRemoveMany(s, m_toRemove);
    setInsertMany(s, std::move(m_toInsert));
  }
};


void TextMCoordMap::LineData::insertBytes(int insStart, int lengthBytes)
{
  insertBytes_spans(insStart, lengthBytes);

  insertBytes_boundaries(m_startsHere, insStart, lengthBytes);
  insertBytes_boundaries(m_endsHere, insStart, lengthBytes);
}


void TextMCoordMap::LineData::insertBytes_spans(
  int insStart, int lengthBytes)
{
  DelayedSetChanges<SingleLineSpan> changes;

  for (SingleLineSpan const &span : m_singleLineSpans) {
    // We will modify `newSpan` to compute the replacement for `span`.
    SingleLineSpan newSpan(span);

    if (insStart <= span.m_startByteIndex) {
      //     [SPAN]
      //  ^
      // ins

      // Shift the span right.
      newSpan.m_startByteIndex += lengthBytes;
      newSpan.m_endByteIndex += lengthBytes;
    }
    else {
      xassert(insStart > span.m_startByteIndex);

      if (insStart > span.m_endByteIndex) {
        // [SPAN]
        //         ^
        //        ins

        // Insertion is beyond `span`, nothing to do.
      }
      else {
        // [SPAN]
        //   ^
        //  ins

        // Shift the right end to the right.
        newSpan.m_endByteIndex += lengthBytes;
      }
    }

    if (newSpan != span) {
      changes.replaceWith(span, std::move(newSpan));
    }
  }

  changes.execute(m_singleLineSpans);
}


/*static*/ void TextMCoordMap::LineData::insertBytes_boundaries(
  std::set<Boundary> &boundaries,
  int insStart,
  int lengthBytes)
{
  DelayedSetChanges<Boundary> changes;

  for (Boundary const &b : boundaries) {
    Boundary newBoundary(b);

    if (insStart <= b.m_byteIndex) {
      //     B
      //  ^
      // ins

      // Shift right.
      newBoundary.m_byteIndex += lengthBytes;

      // For the case where `insStart == b.m_byteIndex`, if this is a
      // start boundary, I think shifting right, and thereby not
      // expanding the span, makes sense.  But for an end boundary, I
      // think it would be more intuitive to *not* shift right, again
      // with the effect of not expanding the range, but this logic
      // will shift and therefore expand.
      //
      // For now I'll keep this behavior since it means both endpoints
      // can be treated the same way, and it probably makes little
      // practical difference, but this might need to be revisited.
    }
    else {
      xassert(insStart > b.m_byteIndex);

      //  B
      //     ^
      //    ins

      // No change needed.
    }

    if (newBoundary != b) {
      changes.replaceWith(b, std::move(newBoundary));
    }
  }

  changes.execute(boundaries);
}


void TextMCoordMap::LineData::deleteBytes(int delStart, int lengthBytes)
{
  deleteBytes_spans(delStart, lengthBytes);

  deleteBytes_boundaries(m_startsHere, delStart, lengthBytes);
  deleteBytes_boundaries(m_endsHere, delStart, lengthBytes);
}


void TextMCoordMap::LineData::deleteBytes_spans(
  int delStart, int lengthBytes)
{
  DelayedSetChanges<SingleLineSpan> changes;

  int delEnd = delStart + lengthBytes;

  for (SingleLineSpan const &span : m_singleLineSpans) {
    // We will modify `newSpan` to compute the replacement for `span`.
    SingleLineSpan newSpan(span);

    if (delStart <= span.m_startByteIndex) {
      if (delEnd <= span.m_startByteIndex) {
        // [DELETION]
        //             [SPAN]

        // Shift the entire span left.
        newSpan.m_startByteIndex -= lengthBytes;
        newSpan.m_endByteIndex -= lengthBytes;
      }
      else if (delEnd >= span.m_endByteIndex) {
        // [ DELETION ]
        //    [SPAN]

        // Collapse the span onto the deletion point.
        newSpan.m_startByteIndex = delStart;
        newSpan.m_endByteIndex = delStart;
      }
      else {
        // [DELETION]
        //       [ SPAN ]

        // Move the left side to the deletion point, and put the right
        // side at deletion point plus overhang.
        int overhang = span.m_endByteIndex - delEnd;
        newSpan.m_startByteIndex = delStart;
        newSpan.m_endByteIndex = delStart + overhang;
      }
    }
    else {
      xassert(delStart > span.m_startByteIndex);

      if (delStart >= span.m_endByteIndex) {
        //         [DELETION]
        // [SPAN]

        // Deletion is beyond `span`, nothing to do.
      }
      else if (delEnd < span.m_endByteIndex) {
        //    [DELETION]
        // [     SPAN     ]

        // Deletion is inside `span`, so move its right end.
        newSpan.m_endByteIndex -= lengthBytes;
      }
      else {
        //     [DELETION]
        // [ SPAN ]

        // Move the right end to the deletion point.
        newSpan.m_endByteIndex = delStart;
      }
    }

    if (newSpan != span) {
      changes.replaceWith(span, std::move(newSpan));
    }
  }

  changes.execute(m_singleLineSpans);
}


/*static*/ void TextMCoordMap::LineData::deleteBytes_boundaries(
  std::set<Boundary> &boundaries,
  int delStart,
  int lengthBytes)
{
  DelayedSetChanges<Boundary> changes;

  int delEnd = delStart + lengthBytes;

  for (Boundary const &b : boundaries) {
    Boundary newBoundary(b);

    if (delStart <= b.m_byteIndex) {
      if (delEnd <= b.m_byteIndex) {
        // [DELETION]
        //             ^
        //          boundary

        // Shift left by the deletion amount.
        newBoundary.m_byteIndex -= lengthBytes;
      }
      else {
        //   [DELETION]
        //         ^
        //      boundary

        // Move the boundary to the deletion start.
        newBoundary.m_byteIndex = delStart;
      }
    }
    else {
      xassert(delStart > b.m_byteIndex);

      //         [DELETION]
      //     ^
      // boundary

      // No change needed.
    }

    if (newBoundary != b) {
      changes.replaceWith(b, std::move(newBoundary));
    }
  }

  changes.execute(boundaries);
}


int TextMCoordMap::LineData::removeEnd_getByteIndex(Value v)
{
  // Since the set is indexed by byte index first, we have to resort to
  // linear search.
  for (auto it = m_endsHere.begin(); it != m_endsHere.end(); ++it) {
    if ((*it).m_value == v) {
      int byteIndex = (*it).m_byteIndex;
      m_endsHere.erase(it);
      return byteIndex;
    }
  }

  xfailure("no endpoint with specified value");
  return 0;        // not reached
}


auto TextMCoordMap::LineData::getLineEntries() const
  -> std::set<LineEntry>
{
  std::set<LineEntry> ret;

  for (SingleLineSpan const &span : m_singleLineSpans) {
    ret.insert(LineEntry(
      span.m_startByteIndex,
      span.m_endByteIndex,
      span.m_value
    ));
  }

  for (Boundary const &b : m_startsHere) {
    ret.insert(LineEntry(
      b.m_byteIndex,
      std::nullopt,
      b.m_value
    ));
  }

  for (Value const &v : m_continuesHere) {
    ret.insert(LineEntry(
      std::nullopt,
      std::nullopt,
      v
    ));
  }

  for (Boundary const &b : m_endsHere) {
    ret.insert(LineEntry(
      std::nullopt,
      b.m_byteIndex,
      b.m_value
    ));
  }

  return ret;
}


std::optional<int> TextMCoordMap::LineData::largestByteIndex() const
{
  std::optional<int> largest = std::nullopt;

  for (SingleLineSpan const &span : m_singleLineSpans) {
    optAccumulateMax(largest, span.m_startByteIndex);
    optAccumulateMax(largest, span.m_endByteIndex);
  }

  for (Boundary const &b : m_startsHere) {
    optAccumulateMax(largest, b.m_byteIndex);
  }

  for (Boundary const &b : m_endsHere) {
    optAccumulateMax(largest, b.m_byteIndex);
  }

  return largest;
}


void TextMCoordMap::LineData::adjustForDocument(
  TextDocumentCore const &doc, LineIndex line)
{
  int docLineBytes = doc.lineLengthBytes(line);

  if (std::optional<int> largestCoordByteIndex = this->largestByteIndex()) {
    int excessBytes = *largestCoordByteIndex - docLineBytes;
    if (excessBytes > 0) {
      // All coordinates larger than `docLineBytes` are invalid.  Delete
      // the intervening characters to collapse the coordinates into
      // `docLineBytes`.
      TRACE1("LineData::adjustForDocument: deleting " << excessBytes <<
             " bytes from line " << line);
      deleteBytes(docLineBytes, excessBytes);
    }
  }
}


void TextMCoordMap::LineData::addEndBoundaryToLastLine(Value v)
{
  // First check for a continuation since that's faster.
  if (setRemove(m_continuesHere, v)) {
    // Add an end to replace the continuation.  (In this class, we have
    // no information about the line length, so cannot make it end at
    // the line end.)
    m_endsHere.insert(Boundary(0, v));
  }

  else {
    // I have to resort to linear search to find the start boundary.
    for (auto it = m_startsHere.begin(); it != m_startsHere.end(); ++it) {
      Boundary const &boundary = *it;
      if (boundary.m_value == v) {
        // Convert the start into a zero-width single-line segment.
        int bi = boundary.m_byteIndex;
        m_singleLineSpans.insert(SingleLineSpan(bi, bi, v));
        m_startsHere.erase(it);
        return;
      }
    }
    xfailure("no start?");
  }
}


// --------------------------- TextMCoordMap ---------------------------
auto TextMCoordMap::getOrCreateLineData(LineIndex line) -> LineData *
{
  validateLineIndex(line);

  m_lineData.ensureValidIndex(line);

  LineData *ret = m_lineData.get(line);
  if (!ret) {
    ret = new LineData();
    m_lineData.replace(line, ret);
  }

  return ret;
}


auto TextMCoordMap::getLineDataC(LineIndex line) const -> LineData const * NULLABLE
{
  if (line < m_lineData.length()) {
    return m_lineData.get(line);
  }
  else {
    return nullptr;
  }
}


auto TextMCoordMap::getLineData(LineIndex line) -> LineData * NULLABLE
{
  return const_cast<LineData * NULLABLE>(getLineDataC(line));
}


void TextMCoordMap::validateLineIndex(LineIndex line) const
{
  if (m_numLines.has_value()) {
    xassert(line < *m_numLines);
  }
}


/*static*/ bool TextMCoordMap::equalLineData(
  LineDataGapArray const &a,
  LineDataGapArray const &b)
{
  if (a.length() != b.length()) {
    return false;
  }

  for (LineIndex i(0); i < a.length(); ++i) {
    LineData const * NULLABLE aLineData = a.get(i);
    LineData const * NULLABLE bLineData = b.get(i);

    if ((aLineData==nullptr) != (bLineData==nullptr)) {
      return false;
    }
    if (aLineData && (*aLineData) != (*bLineData)) {
      return false;
    }
  }

  return true;
}


TextMCoordMap::~TextMCoordMap()
{
  // This has to be done explicitly because `m_lineData` does not know
  // it owns the objects it points at.
  clearEntries();
}


TextMCoordMap::TextMCoordMap(TextMCoordMap const &obj)
  : DMEMB(m_values),
    m_lineData(),
    DMEMB(m_numLines)
{
  m_lineData.insertManyZeroes(LineIndex(0), obj.m_lineData.length());
  for (LineIndex i(0); i < obj.m_lineData.length(); ++i) {
    if (LineData const *lineData = obj.getLineDataC(i)) {
      m_lineData.set(i, new LineData(*lineData));
    }
  }

  selfCheck();
}


// Assert that `opt` is positive if it has a value.
static void xassertOptPositive(std::optional<int> const &opt)
{
  if (opt.has_value()) {
    xassert(*opt > 0);
  }
}


TextMCoordMap::TextMCoordMap(std::optional<int> numLines)
  : m_values(),
    m_lineData(),
    IMEMBFP(numLines)
{
  xassertOptPositive(numLines);
  selfCheck();
}


bool TextMCoordMap::operator==(TextMCoordMap const &obj) const
{
  return EMEMB(m_values) &&
         EMEMB(m_numLines) &&
         equalLineData(m_lineData, obj.m_lineData);
}


void TextMCoordMap::selfCheck() const
{
  EXN_CONTEXT("selfCheck");

  if (m_numLines.has_value()) {
    xassert(*m_numLines >= 1);
    xassert(m_lineData.length() <= *m_numLines);
  }

  // All values we have seen in `m_lineData`.
  std::set<Value> seenValues;

  // Values for which we have seen the start but not the end.
  std::set<Value> activeValues;

  for (LineIndex i(0); i < m_lineData.length(); ++i) {
    if (LineData const *lineData = getLineDataC(i)) {
      for (SingleLineSpan const &span : lineData->m_singleLineSpans) {
        span.selfCheck();

        setInsertUnique(seenValues, span.m_value);
      }

      std::set<Value> startedValues;
      for (Boundary const &b : lineData->m_startsHere) {
        b.selfCheck();

        // This should be the first time seeing this value.
        setInsertUnique(seenValues, b.m_value);

        // Accumulate all of the values started here.
        setInsertUnique(startedValues, b.m_value);
      }

      std::set<Value> continuedValues;
      for (Value const &v : lineData->m_continuesHere) {
        // Continuations should already have been seen.
        xassert(contains(seenValues, v));

        // And they should be registered as active.
        xassert(contains(activeValues, v));

        xassert(!contains(startedValues, v));

        setInsertUnique(continuedValues, v);
      }

      std::set<Value> endedValues;
      for (Boundary const &b : lineData->m_endsHere) {
        b.selfCheck();

        xassert(contains(seenValues, b.m_value));
        xassert(contains(activeValues, b.m_value));

        xassert(!contains(startedValues, b.m_value));
        xassert(!contains(continuedValues, b.m_value));

        setInsertUnique(endedValues, b.m_value);
      }

      // Every previously-active value should be continued or ended.
      xassert(setUnion(continuedValues, endedValues) == activeValues);

      // Deactivate all ended values.
      setRemoveMany(activeValues, endedValues);

      // Activate all started values.
      setInsertMany(activeValues, std::move(startedValues));
    }

    else /*lineData==nullptr*/ {
      // A missing entry means all the sets are all empty, which can
      // only happen if there are no active multi-line spans.
      xassert(activeValues.empty());
    }
  }

  // The set of all values seen in `m_lineData` should match what we
  // have tracked in `m_values`.
  xassert(seenValues == m_values);
}


void TextMCoordMap::insertEntry(DocEntry entry)
{
  xassert(validRange(entry.m_range));

  setInsertUnique(m_values, entry.m_value);

  // Slightly shorter name for convenience.
  TextMCoordRange const &range = entry.m_range;

  if (range.m_start.m_line == range.m_end.m_line) {
    getOrCreateLineData(range.m_start.m_line)->
      m_singleLineSpans.insert(
        SingleLineSpan(range.m_start.m_byteIndex,
                       range.m_end.m_byteIndex,
                       entry.m_value));
  }

  else /*multi-line range*/ {
    getOrCreateLineData(range.m_start.m_line)->
      m_startsHere.insert(
        Boundary(range.m_start.m_byteIndex, entry.m_value));

    for (LineIndex line = range.m_start.m_line+1;
         line < range.m_end.m_line;
         ++line) {
      getOrCreateLineData(line)->
        m_continuesHere.insert(entry.m_value);
    }

    getOrCreateLineData(range.m_end.m_line)->
      m_endsHere.insert(
        Boundary(range.m_end.m_byteIndex, entry.m_value));
  }
}


void TextMCoordMap::clearEntries()
{
  m_values.clear();

  for (LineIndex i(0); i < m_lineData.length(); ++i) {
    if (LineData *data = m_lineData.get(i)) {
      delete data;
      m_lineData.replace(i, nullptr);
    }
  }

  // Having removed all of the `LineData` objects, clear the array as
  // well in order to ensure `maxEntryLine()==-1`.
  m_lineData.clear();

  selfCheck();
}


void TextMCoordMap::clearEverything(std::optional<int> numLines)
{
  xassertOptPositive(numLines);

  clearEntries();
  m_numLines = numLines;

  selfCheck();
}


void TextMCoordMap::confineLineIndices(int maxNumLines)
{
  xassertPrecondition(maxNumLines > 0);

  int numDiagLines = maxEntryLine() + 1;

  int excessLines = numDiagLines - maxNumLines;
  if (excessLines > 0) {
    // Adjust by deleting extra lines.
    TRACE1("adjustForDocument: deleting " << excessLines <<
           " lines at the end of the (virtual) document so its" <<
           " line count drops from " << numDiagLines <<
           " to " << maxNumLines);
    deleteLines(LineIndex(maxNumLines-1), excessLines);

    // After the deletion, the number of lines according to the
    // diagnostics should be the same as the number according to the
    // document.
    xassert(maxEntryLine()+1 == maxNumLines);
  }
}


void TextMCoordMap::adjustForDocument(TextDocumentCore const &doc)
{
  confineLineIndices(doc.numLines());

  // Confine the line lengths.
  for (LineIndex i(0); i < m_lineData.length(); ++i) {
    if (LineData *data = m_lineData.get(i)) {
      data->adjustForDocument(doc, i);
    }
  }

  // Set `m_numLines` to match `doc`.
  m_numLines = doc.numLines();
}


void TextMCoordMap::setNumLinesAndAdjustAccordingly(int numLines)
{
  confineLineIndices(numLines);
  m_numLines = numLines;
}


void TextMCoordMap::insertLines(LineIndex line, int count)
{
  xassert(canTrackUpdates());

  // Following the rules of `TextDocumentCore::insertLine`, it is
  // permissible to "insert" a line right after the last one,
  // effectively appending new lines.
  xassert(line <= *m_numLines);

  xassert(count >= 0);
  *m_numLines += count;

  // Above the insertion point.
  LineData const * NULLABLE lineAbove =
    line.isPositive()?
      getLineDataC(line.nzpred()) :
      nullptr;

  // Insert blank entries in the array.
  if (line < m_lineData.length()) {
    m_lineData.insertManyZeroes(line, count);
  }
  else {
    // We should have no need for empty entries since `line` is after
    // any entries.
    return;
  }

  if (!lineAbove) {
    // No data to spread.
    return;
  }

  // Populate them with continuations of spans active from the line
  // above.
  for (LineIndex i=line; i < line+count; ++i) {
    LineData *lineData = getOrCreateLineData(i);

    for (Boundary const &b : lineAbove->m_startsHere) {
      lineData->m_continuesHere.insert(b.m_value);
    }

    for (Value const &v : lineAbove->m_continuesHere) {
      lineData->m_continuesHere.insert(v);
    }
  }
}


void TextMCoordMap::deleteLines(LineIndex line, int count)
{
  xassert(canTrackUpdates());

  xassert(cc::le_le(line, line+count, LineIndex(*m_numLines)));
  //                    <=          <=

  // m_numLines gets decreased during the loop below.

  // Single line spans in the deleted region, which will be deposited
  // at the start of the line that ends up at index `line`.
  std::set<SingleLineSpan> singleLineSpans;

  // Values whose span started or ended in the deleted region.
  std::set<Value> starts;
  std::set<Value> ends;

  for (int i=0; i < count; ++i) {
    // The line at `line` will be removed.
    if (LineData *lineData = getLineData(line)) {
      setInsertMany(singleLineSpans,
        std::move(lineData->m_singleLineSpans));

      for (Boundary const &b : lineData->m_startsHere) {
        setInsertUnique(starts, b.m_value);
      }

      // The continuations don't matter.

      for (Boundary const &b : lineData->m_endsHere) {
        setInsertUnique(ends, b.m_value);
      }

      // We have everything we need, discard the line object.
      delete lineData;
    }

    // And remove its pointer from the table, shifting the indices so
    // the next line will be at index `line`.
    if (line < m_lineData.length()) {
      m_lineData.remove(line);
    }

    // Reduce the total number of lines.
    --*m_numLines;
    xassert(*m_numLines >= 1);
  }

  if (singleLineSpans.empty() &&
      starts.empty() &&
      ends.empty()) {
    // There was nothing in the range, so nothing to do.
    return;
  }

  // Now we need to put everything we collected from the deleted lines
  // onto `line`, unless we just deleted the last line, in which case it
  // goes to the line above.
  xassert(line <= *m_numLines);
  LineData *recipientLine =
    // This test of `m_numLines` is the primary reason it exists.
    line < *m_numLines?
      getOrCreateLineData(line) :          // Line after deletion range.
      getOrCreateLineData(line.nzpred());  // Line before deletion range.

  for (SingleLineSpan const &span : singleLineSpans) {
    recipientLine->m_singleLineSpans.insert(
      SingleLineSpan(0, 0, span.m_value));
  }

  for (Value const &v : starts) {
    if (contains(ends, v)) {
      // This value's range started and ended in the deleted section
      // (above), so put it all on this (the next) line.
      recipientLine->m_singleLineSpans.insert(
        SingleLineSpan(0, 0, v));
    }

    else if (contains(recipientLine->m_continuesHere, v)) {
      // Replace the continuation with a start.
      setRemoveExisting(recipientLine->m_continuesHere, v);
      recipientLine->m_startsHere.insert(
        Boundary(0, v));
    }

    else {
      // The span must have previously ended here.
      int endByteIndex = recipientLine->removeEnd_getByteIndex(v);

      // Replace the end with a single-line span.
      recipientLine->m_singleLineSpans.insert(
        SingleLineSpan(0, endByteIndex, v));
    }
  }

  for (Value const &v : ends) {
    if (contains(starts, v)) {
      // Already dealt with it in the previous loop.
    }
    else if (line < *m_numLines) {
      // We can be sure that the value did not start or continue on
      // `recipientLine`, so just add an end.
      recipientLine->m_endsHere.insert(
        Boundary(0, v));
    }
    else {
      // In the case of deleting the last line, we need more complicated
      // logic to handle it.
      recipientLine->addEndBoundaryToLastLine(v);
    }
  }
}


void TextMCoordMap::insertLineBytes(TextMCoord tc, int lengthBytes)
{
  xassert(canTrackUpdates());

  if (LineData *lineData = getLineData(tc.m_line)) {
    lineData->insertBytes(tc.m_byteIndex, lengthBytes);
  }
}


void TextMCoordMap::deleteLineBytes(TextMCoord tc, int lengthBytes)
{
  xassert(canTrackUpdates());

  if (LineData *lineData = getLineData(tc.m_line)) {
    lineData->deleteBytes(tc.m_byteIndex, lengthBytes);
  }
}


bool TextMCoordMap::empty() const
{
  return m_values.empty();
}


int TextMCoordMap::numEntries() const
{
  return safeToInt(m_values.size());
}


int TextMCoordMap::maxEntryLine() const
{
  int len = m_lineData.length();
  return len - 1;
}


std::optional<int> TextMCoordMap::getNumLinesOpt() const
{
  return m_numLines;
}


int TextMCoordMap::getNumLines() const
{
  xassert(canTrackUpdates());

  return m_numLines.value();
}


bool TextMCoordMap::canTrackUpdates() const
{
  return m_numLines.has_value();
}


bool TextMCoordMap::validCoord(TextMCoord tc) const
{
  if (m_numLines.has_value()) {
    if (!( tc.m_line < *m_numLines )) {
      return false;
    }
  }

  return true;
}


bool TextMCoordMap::validRange(TextMCoordRange const &range) const
{
  return validCoord(range.m_start) &&
         validCoord(range.m_end) &&
         range.isRectified();
}


auto TextMCoordMap::getLineEntries(LineIndex line) const -> std::set<LineEntry>
{
  if (LineData const *lineData = getLineDataC(line)) {
    return lineData->getLineEntries();
  }
  else {
    return {};
  }
}


auto TextMCoordMap::getAllEntries() const -> std::set<DocEntry>
{
  std::set<DocEntry> ret;

  // Map from associated value to the start coordinate of all of the
  // spans for which we have seen the start but not the end.
  std::map<Value, TextMCoord> openSpans;

  for (LineIndex line(0); line <= maxEntryLine(); ++line) {
    if (LineData const *lineData = getLineDataC(line)) {

      for (SingleLineSpan const &span : lineData->m_singleLineSpans) {
        ret.insert(
          DocEntry(
            TextMCoordRange(
              TextMCoord(line, span.m_startByteIndex),
              TextMCoord(line, span.m_endByteIndex)
            ),
            span.m_value
          ));
      }

      for (Boundary const &b : lineData->m_startsHere) {
        openSpans.insert({b.m_value,
          TextMCoord(line, b.m_byteIndex)});
      }

      // The continuations aren't important here.

      for (Boundary const &b : lineData->m_endsHere) {
        // Extract the start coordinate.
        TextMCoord startPt = mapMoveValueAt(openSpans, b.m_value);

        ret.insert(
          DocEntry(
            TextMCoordRange(
              startPt,
              TextMCoord(line, b.m_byteIndex)
            ),
            b.m_value
          ));
      }
    }
  }

  xassert(openSpans.empty());

  return ret;
}


auto TextMCoordMap::getMappedValues() const -> std::set<Value>
{
  return m_values;
}


TextMCoordMap::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TextMCoordMap"_sym);
  GDV_WRITE_MEMBER_SYM(m_numLines);
  m.mapSetValueAtSym("entries", toGDValue(getAllEntries()));
  return m;
}


gdv::GDValue TextMCoordMap::dumpInternals() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP);
  m.taggedContainerSetTag("TextMCoordMapInternals"_sym);

  GDV_WRITE_MEMBER_SYM(m_numLines);
  GDV_WRITE_MEMBER_SYM(m_values);

  // Build a map containing `m_lineData`.  (Unfortunately, C++
  // access control rules prohibit easily factoring this into its own
  // function.)
  {
    GDValue ldm(GDVK_MAP);

    for (LineIndex i(0); i < m_lineData.length(); ++i) {
      if (LineData const *lineData = m_lineData.get(i)) {
        ldm.mapSetValueAt(i, toGDValue(*lineData));
      }
    }

    // Include the length as well, since it's not otherwise visible.
    ldm.mapSetValueAtSym("length", m_lineData.length());

    // Add it to the outer `m`.
    m.mapSetValueAtSym("lineData", std::move(ldm));
  }

  return m;
}


// EOF
