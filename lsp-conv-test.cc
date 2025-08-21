// lsp-conv-test.cc
// Tests for `lsp-conv` module.

#include "lsp-conv.h"                  // module under test
#include "unit-tests.h"                // decl for my entry point

#include "lsp-data.h"                  // LSP_TextDocumentContentChangeEvent
#include "named-td.h"                  // NamedTextDocument
#include "range-text-repl.h"           // RangeTextReplacement
#include "td-change-seq.h"             // TextDocumentChangeSequence
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentObservationRecorder

#include "smbase/gdvalue.h"            // gdv::toGDValue for TEST_CASE_EXPRS
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, smbase_loopi
#include "smbase/sm-test.h"            // EXPECT_EQ

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <utility>                     // std::move
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// Pair of docs and a recorder.
class TDCorePair {
public:      // data
  // The document we will change directly.  It has an observation
  // recorder inside it that we will be using to replay changes onto
  // `m_secondaryDoc`.
  NamedTextDocument m_primaryDoc;

  // This one will be changed indirectly by recording changes to the
  // primary, then sending them through the conversion cycle, and
  // finally applying them.
  TextDocumentCore m_secondaryDoc;

  // Diagnostics ostensibly derived from the lastest tracked version,
  // although in reality just empty; this is part of the protocol used
  // by the recorder to stay in sync.  Never null.
  std::unique_ptr<TextDocumentDiagnostics> m_tdd;

  // Another copy of the primary, which is also directly modified in
  // lock step with `m_primaryDoc`, but using different code for
  // interpreting `TextDocumentChangeSequence`.
  TextDocumentCore m_shadowPrimaryDoc;

public:      // methods
  TDCorePair()
    : m_primaryDoc(),
      m_secondaryDoc(),
      m_tdd()
  {
    // Steady state is we are tracking.
    m_primaryDoc.beginTrackingChanges();

    makeDiagnostics();

    selfCheck();
  }

  void selfCheck() const
  {
    m_primaryDoc.selfCheck();
    xassert(m_primaryDoc.trackingChanges());

    m_secondaryDoc.selfCheck();

    xassert(m_tdd);
    m_tdd->selfCheck();

    m_shadowPrimaryDoc.selfCheck();
    if (m_shadowPrimaryDoc != m_primaryDoc.getCore()) {
      // Print more detail.
      EXPECT_EQ(doubleQuote(m_shadowPrimaryDoc.getWholeFileString()),
                doubleQuote(m_primaryDoc.getWholeFileString()));
    }
    xassert(m_shadowPrimaryDoc == m_primaryDoc.getCore());
  }

  // Make empty diagnostics corresponding to the current version of
  // `m_primaryDoc`.
  void makeDiagnostics()
  {
    m_tdd.reset(new TextDocumentDiagnostics(
      m_primaryDoc.getVersionNumber(), m_primaryDoc.numLines()));
  }

  // After having made changes to `m_primaryDoc`, replay them against
  // `m_secondaryDoc` and check for equality.
  void syncAfterChange()
  {
    // Get recorded changes.
    RCSerf<TextDocumentChangeSequence const> recordedChanges =
      m_primaryDoc.getUnsentChanges();
    VPVAL(toGDValue(*recordedChanges).asIndentedString());

    // Convert to LSP.
    std::list<LSP_TextDocumentContentChangeEvent> lspChanges =
      convertRecordedChangesToLSPChanges(*recordedChanges);
    LSP_DidChangeTextDocumentParams lspParams(
      LSP_VersionedTextDocumentIdentifier("irrelevant", 1),
      std::move(lspChanges));
    VPVAL(toGDValue(lspParams).asIndentedString());

    // We're done with these.
    recordedChanges.reset();

    // Apply LSP to secondary.
    applyLSPDocumentChanges(lspParams, m_secondaryDoc);

    // Verify secondary agrees with primary.  (This gets the strings and
    // quotes them, as opposed to using operator==, so the output in the
    // case of a difference is informative.)
    EXPECT_EQ(doubleQuote(m_secondaryDoc.getWholeFileString()),
              doubleQuote(m_primaryDoc.getWholeFileString()));

    // Bring the recorder up to date.
    m_primaryDoc.updateDiagnostics(std::move(m_tdd));
    m_primaryDoc.beginTrackingChanges();

    // Prepare for the next cycle.
    makeDiagnostics();

    selfCheck();
  }

  void test_replaceWhole()
  {
    m_primaryDoc.replaceWholeFileString("zero\none\ntwo\n");
    m_shadowPrimaryDoc.replaceWholeFileString("zero\none\ntwo\n");
    syncAfterChange();
  }

  void testOne_replaceMultilineRange(
    int startLine,
    int startByteIndex,
    int endLine,
    int endByteIndex,
    char const *text,
    char const *expect)
  {
    TEST_CASE_EXPRS("testOne_replaceMultilineRange",
      startLine,
      startByteIndex,
      endLine,
      endByteIndex,
      text);

    m_primaryDoc.replaceMultilineRange(
      TextMCoordRange(
        TextMCoord(startLine, startByteIndex),
        TextMCoord(endLine, endByteIndex)),
      text);
    EXPECT_EQ(m_primaryDoc.getWholeFileString(), expect);

    m_shadowPrimaryDoc.replaceMultilineRange(
      TextMCoordRange(
        TextMCoord(startLine, startByteIndex),
        TextMCoord(endLine, endByteIndex)),
      text);

    syncAfterChange();
  }

  // This is adapted from `td-core-test.cc`.
  void test_replaceMultilineRange()
  {
    EXPECT_EQ(m_primaryDoc.getWholeFileString(), "");

    testOne_replaceMultilineRange(0,0,0,0, "zero\none\n",
      "zero\n"
      "one\n");

    testOne_replaceMultilineRange(2,0,2,0, "two\nthree\n",
      "zero\n"
      "one\n"
      "two\n"
      "three\n");

    testOne_replaceMultilineRange(1,1,2,2, "XXXX\nYYYY",
      "zero\n"
      "oXXXX\n"
      "YYYYo\n"
      "three\n");

    testOne_replaceMultilineRange(0,4,3,0, "",
      "zerothree\n");

    testOne_replaceMultilineRange(0,9,1,0, "",
      "zerothree");

    testOne_replaceMultilineRange(0,2,0,3, "0\n1\n2\n3",
      "ze0\n"
      "1\n"
      "2\n"
      "3othree");
  }

  void makeRandomEdit()
  {
    TextDocumentChangeSequence changes =
      makeRandomChange(m_primaryDoc.getCore());

    changes.applyToDocCore(m_shadowPrimaryDoc);
    changes.applyToDocument(m_primaryDoc);
  }
};


void test_replace()
{
  {
    TDCorePair docs;
    docs.test_replaceWhole();
  }

  {
    TDCorePair docs;
    docs.test_replaceMultilineRange();
  }
}


void test_randomEdits()
{
  int const outerLimit =
    envRandomizedTestIters(10, "LCT_OUTER_LIMIT", 2);
  int const innerLimit =
    envRandomizedTestIters(200, "LCT_INNER_LIMIT", 2);

  for (int outer=0; outer < outerLimit; ++outer) {
    EXN_CONTEXT_EXPR(outer);

    TDCorePair docs;

    for (int inner=0; inner < innerLimit; ++inner) {
      EXN_CONTEXT_EXPR(inner);

      docs.makeRandomEdit();
      docs.syncAfterChange();
    }
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_conv(CmdlineArgsSpan args)
{
  test_replace();
  test_randomEdits();
}


// EOF
