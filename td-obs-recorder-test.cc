// td-obs-recorder-test.cc
// Tests for `td-obs-recorder` module.

#include "unit-tests.h"                // decl for my entry point

#include "td-obs-recorder.h"           // module under test

#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/gdv-ordered-map.h"    // gdv::OrderedMap
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, TEST_CASE

using namespace gdv;


typedef TextDocumentCore::VersionNumber VersionNumber;


OPEN_ANONYMOUS_NAMESPACE


void checkFile(TextDocumentCore const &doc, char const *expect)
{
  EXPECT_EQ(vectorOfUCharToString(doc.getWholeFile()), expect);
}


void test_basics()
{
  TEST_CASE("test_basics");

  TextDocumentCore doc;
  checkFile(doc, "");
  TextDocumentObservationRecorder recorder(doc);
  EXPECT_EQ(recorder.trackingSomething(), false);
  EXPECT_EQ(recorder.getEarliestVersion().has_value(), false);
  EXPECT_EQ(recorder.isTracking(0), false);
  EXPECT_EQ(toGDValue(recorder), GDValue(GDVMap{}));

  // Make a change while not tracking anything.
  doc.insertLine(0);
  checkFile(doc, "\n");
  EXPECT_EQ(recorder.trackingSomething(), false);
  EXPECT_EQ(toGDValue(recorder), GDValue(GDVMap{}));

  // Begin tracking.
  VersionNumber ver1 = doc.getVersionNumber();
  recorder.beginTracking(ver1);
  EXPECT_EQ(recorder.trackingSomething(), true);
  EXPECT_EQ(recorder.getEarliestVersion().value(), ver1);
  EXPECT_EQ(recorder.isTracking(ver1), true);
  EXPECT_EQ(toGDValue(recorder), GDValue(GDVMap{
    { ver1, GDVSequence{} },
  }));

  // Make a change while tracking is enabled.
  doc.insertLine(0);
  checkFile(doc, "\n\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    {
      ver1, GDVSequence{
        GDVTaggedOrderedMap("InsertLine"_sym, {
          GDVMapEntry("line"_sym, 0),
        }),
      }
    },
  })));

  // Switch to GDVN-based expectation for more convenient notation.
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0]]") },
  })));

  // Insert some text.
  doc.insertString(TextMCoord(0, 0), "hello");
  checkFile(doc, "hello\n\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0] "
                      "InsertText[tc:MC(0 0) text:\"hello\"]]") },
  })));

  // Delete text.
  doc.deleteTextBytes(TextMCoord(0, 1), 2);
  checkFile(doc, "hlo\n\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0] "
                      "InsertText[tc:MC(0 0) text:\"hello\"] "
                      "DeleteText[tc:MC(0 1) lengthBytes:2]]") },
  })));

  // Delete remainder of text on that line, since that is required
  // before we can delete the line itself.
  doc.deleteTextBytes(TextMCoord(0, 0), 3);
  checkFile(doc, "\n\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0] "
                      "InsertText[tc:MC(0 0) text:\"hello\"] "
                      "DeleteText[tc:MC(0 1) lengthBytes:2] "
                      "DeleteText[tc:MC(0 0) lengthBytes:3]]") },
  })));


  // Delete the line now that it is empty.
  doc.deleteLine(0);
  checkFile(doc, "\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0] "
                      "InsertText[tc:MC(0 0) text:\"hello\"] "
                      "DeleteText[tc:MC(0 1) lengthBytes:2] "
                      "DeleteText[tc:MC(0 0) lengthBytes:3] "
                      "DeleteLine[line:0]]") },
  })));
  EXPECT_EQ(recorder.getEarliestVersion().value(), ver1);

  // Track a new version.
  VersionNumber ver2 = doc.getVersionNumber();
  recorder.beginTracking(ver2);
  EXPECT_EQ(recorder.trackingSomething(), true);
  EXPECT_EQ(recorder.getEarliestVersion().value(), ver1);  // ver1 is still earliest
  EXPECT_EQ(recorder.isTracking(ver1), true);
  EXPECT_EQ(recorder.isTracking(ver2), true);

  // Insert a few lines.
  doc.insertLine(0);
  doc.insertLine(1);
  doc.insertLine(2);
  checkFile(doc, "\n\n\n\n");
  EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    { ver1, fromGDVN("[InsertLine[line:0] "
                      "InsertText[tc:MC(0 0) text:\"hello\"] "
                      "DeleteText[tc:MC(0 1) lengthBytes:2] "
                      "DeleteText[tc:MC(0 0) lengthBytes:3] "
                      "DeleteLine[line:0]]") },
    { ver2, fromGDVN("[InsertLine[line:0] "
                      "InsertLine[line:1] "
                      "InsertLine[line:2]]") },
  })));

  {
    // Make some diagnostics that could have applied to `ver1`.
    TextDocumentDiagnostics diagnostics(ver1);
    diagnostics.insertDiagnostic({{1,0}, {1,0}}, TDD_Diagnostic("msg"));
    EXPECT_EQ(toGDValue(diagnostics), fromGDVN( "{"
      "TDD_DocEntry["
        "range:MCR(MC(1 0) MC(1 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg\" related:[]]"
      "]"
    "}"));

    // Roll them forward.
    recorder.applyChangesToDiagnostics(&diagnostics);

    // That should have removed the changes recorded on top of `ver1`, but
    // kept the changes for `ver2`.
    EXPECT_EQ(recorder.trackingSomething(), true);
    EXPECT_EQ(recorder.getEarliestVersion().value(), ver2);
    EXPECT_EQ(recorder.isTracking(ver1), false);
    EXPECT_EQ(recorder.isTracking(ver2), true);
    EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
      { ver2, fromGDVN("[InsertLine[line:0] "
                        "InsertLine[line:1] "
                        "InsertLine[line:2]]") },
    })));

    // It should have modified the diagnostic, pushing it down to line 4;
    // the `ver1` changes are a no-op, but we also apply the `ver2`
    // changes since the goal is to bring the diagnostics up to the
    // current version in `doc`.
    EXPECT_EQ(toGDValue(diagnostics), fromGDVN( "{"
      "TDD_DocEntry["
        "range:MCR(MC(4 0) MC(4 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg\" related:[]]"
      "]"
    "}"));
  }

  {
    // Make some diagnostics that could have applied to `ver2`.  Since
    // it is textually the same as `ver1`, we use similar diagnostics.
    TextDocumentDiagnostics diagnostics(ver2);
    diagnostics.insertDiagnostic({{0,0}, {0,0}}, TDD_Diagnostic("msg0"));
    diagnostics.insertDiagnostic({{1,0}, {1,0}}, TDD_Diagnostic("msg1"));
    EXPECT_EQ(toGDValue(diagnostics), fromGDVN( "{"
      "TDD_DocEntry["
        "range:MCR(MC(0 0) MC(0 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg0\" related:[]]"
      "]"
      "TDD_DocEntry["
        "range:MCR(MC(1 0) MC(1 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg1\" related:[]]"
      "]"
    "}"));

    // Roll them forward.
    recorder.applyChangesToDiagnostics(&diagnostics);

    // That should have removed the changes recorded on top of `ver2`.
    EXPECT_EQ(recorder.trackingSomething(), false);
    EXPECT_EQ(recorder.getEarliestVersion().has_value(), false);
    EXPECT_EQ(recorder.isTracking(ver1), false);
    EXPECT_EQ(recorder.isTracking(ver2), false);
    EXPECT_EQ(toGDValue(recorder), (GDValue(GDVMap{
    })));

    // It should have modified the diagnostics.
    EXPECT_EQ(toGDValue(diagnostics), fromGDVN( "{"
      "TDD_DocEntry["
        "range:MCR(MC(3 0) MC(3 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg0\" related:[]]"
      "]"
      "TDD_DocEntry["
        "range:MCR(MC(4 0) MC(4 0)) "
        "diagnostic:TDD_Diagnostic[message:\"msg1\" related:[]]"
      "]"
    "}"));
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_obs_recorder(CmdlineArgsSpan args)
{
  test_basics();

  // TODO: More!
}


// EOF
