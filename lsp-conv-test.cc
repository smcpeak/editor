// lsp-conv-test.cc
// Tests for `lsp-conv` module.

#include "lsp-conv.h"                  // module under test
#include "unit-tests.h"                // decl for my entry point

#include "lsp-data.h"                  // LSP_TextDocumentContentChangeEvent
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentObservationRecorder

#include "smbase/gdvalue.h"            // gdv::toGDValue for TEST_CASE_EXPRS
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <utility>                     // std::move

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// Pair of docs and a recorder.
class TDCorePair {
public:      // data
  // The document we will change directly.
  TextDocumentCore m_primaryDoc;

  // Recorder watching the primary.
  TextDocumentObservationRecorder m_recorder;

  // This one will be changed indirectly by recording changes to the
  // primary, then sending them through the conversion cycle, and
  // finally applying them.
  TextDocumentCore m_secondaryDoc;

  // Diagnostics ostensibly derived from the lastest tracked version,
  // although in reality just empty; this is part of the protocol used
  // by the recorder to stay in sync.
  std::unique_ptr<TextDocumentDiagnostics> m_tdd;

public:      // methods
  TDCorePair()
    : m_primaryDoc(),
      m_recorder(m_primaryDoc),
      m_secondaryDoc(),
      m_tdd()
  {
    // Steady state is we are tracking.
    m_recorder.beginTrackingCurrentDoc();

    makeDiagnostics();
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
    TextDocumentChangeSequence const &recordedChanges =
      m_recorder.getUnsentChanges();
    VPVAL(toGDValue(recordedChanges).asIndentedString());

    // Convert to LSP.
    std::list<LSP_TextDocumentContentChangeEvent> lspChanges =
      convertRecordedChangesToLSPChanges(recordedChanges);
    LSP_DidChangeTextDocumentParams lspParams(
      LSP_VersionedTextDocumentIdentifier("irrelevant", 1),
      std::move(lspChanges));
    VPVAL(toGDValue(lspParams).asIndentedString());

    // Apply LSP to secondary.
    applyLSPDocumentChanges(lspParams, m_secondaryDoc);

    // Verify secondary agrees with primary.  (This gets the strings and
    // quotes them, as opposed to using operator==, so the output in the
    // case of a difference is informative.)
    EXPECT_EQ(doubleQuote(m_secondaryDoc.getWholeFileString()),
              doubleQuote(m_primaryDoc.getWholeFileString()));

    // Bring the recorder up to date.
    m_recorder.applyChangesToDiagnostics(m_tdd.get());
    m_recorder.beginTrackingCurrentDoc();

    // Prepare for the next cycle.
    makeDiagnostics();
  }

  void test_replaceWhole()
  {
    m_primaryDoc.replaceWholeFileString("zero\none\ntwo\n");
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
};


void test_corePair()
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


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_conv(CmdlineArgsSpan args)
{
  test_corePair();
}


// EOF
