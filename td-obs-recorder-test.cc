// td-obs-recorder-test.cc
// Tests for `td-obs-recorder` module.

#include "unit-tests.h"                // decl for my entry point

#include "td-obs-recorder.h"           // module under test

#include "named-td.h"                  // NamedTextDocument
#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/gdv-ordered-map.h"    // gdv::OrderedMap
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // smbase::{mapKeySet, mapFirstKeyOpt}
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ[_GDV}, TEST_CASE

#include <set>                         // std::set

using namespace gdv;
using namespace smbase;


typedef TextDocumentCore::VersionNumber VersionNumber;
typedef std::set<VersionNumber> VersionSet;


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
  EXPECT_EQ_GDV(recorder.getTrackedVersions(), VersionSet{});
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
  EXPECT_EQ_GDV(recorder.getTrackedVersions(), VersionSet{ver1});
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
  EXPECT_EQ_GDV(recorder.getTrackedVersions(), (VersionSet{ver1, ver2}));

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
    EXPECT_EQ_GDV(recorder.getTrackedVersions(), VersionSet{ver2});
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
    EXPECT_EQ_GDV(recorder.getTrackedVersions(), VersionSet{});
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


// One set of diagnostics that gets eagerly updated, and another that
// gets updated later.
class EagerAndDelayedDiags {
public:      // data
  // Diagnostics kept up to date.
  TextDocumentDiagnostics m_eagerDiags;

  // Keeps `m_eagerDiags` up to date as the associated document changes.
  TextDocumentDiagnosticsUpdater m_updater;

  // These are not updated until we simulate receiving a response from
  // the server.
  TextDocumentDiagnostics m_delayedDiags;

public:      // methods
  ~EagerAndDelayedDiags()
  {}

  EagerAndDelayedDiags(EagerAndDelayedDiags const &obj)
    : DMEMB(m_eagerDiags),
      m_updater(&m_eagerDiags, obj.m_updater.getDocument()),
      DMEMB(m_delayedDiags)
  {
    selfCheck();
  }

  EagerAndDelayedDiags(TextDocumentDiagnostics const &diags,
                       NamedTextDocument const *doc)
    : m_eagerDiags(diags),
      m_updater(&m_eagerDiags, doc),
      m_delayedDiags(diags)
  {
    selfCheck();
  }

  void selfCheck() const
  {
    m_eagerDiags.selfCheck();
    m_updater.selfCheck();
    m_delayedDiags.selfCheck();

    xassert(m_eagerDiags.getOriginVersion() ==
            m_delayedDiags.getOriginVersion());
  }

  VersionNumber getOriginVersion() const
  {
    return m_eagerDiags.getOriginVersion();
  }
};


// A document, its current diagnostics, a recorder, and some historical
// diagnostics that the recorder should be able to roll forward to match
// the current diagnostics.
class DocDiagsRecorderHistory {
  // Not needed, would require some thought.
  NO_OBJECT_COPIES(DocDiagsRecorderHistory);

public:      // data
  // The document to which diagnostics apply and that is being changed.
  NamedTextDocument m_doc;

  // Records changes to `m_doc` in a replayable format.
  TextDocumentObservationRecorder m_recorder;

  // Diagnostics pairs for some previous document versions.
  std::map<VersionNumber, EagerAndDelayedDiags> m_verToDiags;

public:      // methods
  ~DocDiagsRecorderHistory()
  {}

  DocDiagsRecorderHistory()
    : m_doc(),
      m_recorder(m_doc.getCore()),
      m_verToDiags()
  {
    selfCheck();
  }

  void selfCheck() const
  {
    m_doc.selfCheck();
    m_recorder.selfCheck();

    for (auto const &kv : m_verToDiags) {
      EagerAndDelayedDiags const &edd = kv.second;

      edd.selfCheck();
    }

    // The set of versions in `m_verToDiags` should be the same as in
    // `m_recorder`.
    VersionSet verSet = mapKeySet(m_verToDiags);
    EXPECT_EQ_GDV(m_recorder.getTrackedVersions(), verSet);

    // Check some other recorder queries.
    EXPECT_EQ(m_recorder.trackingSomething(), !verSet.empty());
    EXPECT_EQ_GDV(m_recorder.getEarliestVersion(),
                  mapFirstKeyOpt(m_verToDiags));
    for (VersionNumber ver : verSet) {
      EXPECT_EQ(m_recorder.isTracking(ver), true);
    }
  }

  void checkFile(char const *contents) const
  {
    ::checkFile(m_doc.getCore(), contents);
  }

  // Save the current version of the diagnostics and tell the recorder
  // to record accumulated changes.
  void saveVersion(TextDocumentDiagnostics const &diags)
  {
    VersionNumber ver = m_doc.getVersionNumber();
    xassert(diags.getOriginVersion() == ver);
    mapInsertUnique(m_verToDiags, ver,
      EagerAndDelayedDiags(diags, &m_doc));
    m_recorder.beginTracking(ver);

    selfCheck();
  }

  // Simulate having just received the diagnostics for `oldVer`: roll
  // the delayed diagnostics forward and check they match the eager
  // ones.
  void checkSavedVersion(VersionNumber oldVer)
  {
    // Discard any older versions.
    while (!m_verToDiags.empty()) {
      auto it = m_verToDiags.begin();
      xassert(it != m_verToDiags.end());
      if ((*it).first < oldVer) {
        m_verToDiags.erase(it);
      }
      else {
        break;
      }
    }

    // Roll forward from `oldVer`.
    {
      // The first in the map should now be `oldVer`.
      auto it = m_verToDiags.begin();
      xassert(it != m_verToDiags.end());
      xassert((*it).first == oldVer);
      EagerAndDelayedDiags &edd = (*it).second;

      // Roll the delayed diagnostics forward.
      m_recorder.applyChangesToDiagnostics(&edd.m_delayedDiags);

      // They should now match the eager diagnostics.
      EXPECT_EQ_GDV(edd.m_delayedDiags, edd.m_eagerDiags);
      xassert(edd.m_delayedDiags == edd.m_eagerDiags);

      // We are done with `oldVer` and its `edd`.
      m_verToDiags.erase(it);
    }

    // Now the recorder's earliest version should be the same as the
    // first in `m_verToDiags`.
    EXPECT_EQ_GDV(m_recorder.getEarliestVersion(),
                  mapFirstKeyOpt(m_verToDiags));

    selfCheck();
  }
};


// Repeat `test_basics` but using `DocDiagsRecorderHistory`.
//
// This exercises the interface a little, in advance of full randomized
// testing.
void test_DDRH()
{
  TEST_CASE("test_DDRH");

  DocDiagsRecorderHistory ddrh;
  ddrh.selfCheck();
  ddrh.checkFile("");

  // Make a change while not tracking anything.
  ddrh.m_doc.appendCStr("\n");
  ddrh.checkFile("\n");

  // Call this version 1.
  VersionNumber ver1 = ddrh.m_doc.getVersionNumber();

  // Make a diagnostic for version 1 and begin tracking changes.
  {
    TextDocumentDiagnostics diagnostics(ver1);
    diagnostics.insertDiagnostic({{1,0}, {1,0}}, TDD_Diagnostic("msg"));
    ddrh.saveVersion(diagnostics);

    EXPECT_EQ(ddrh.m_recorder.trackingSomething(), true);
    EXPECT_EQ(ddrh.m_recorder.isTracking(ver1), true);
  }

  // Make a change while tracking is enabled.
  ddrh.m_doc.insertAt(TextMCoord(0,0), "\n", 1);
  ddrh.checkFile("\n\n");

  // Insert some text.
  ddrh.m_doc.insertAt(TextMCoord(0, 0), "hello", 5);
  ddrh.checkFile("hello\n\n");

  // Delete text.
  ddrh.m_doc.deleteAt(TextMCoord(0, 1), 2);
  ddrh.checkFile("hlo\n\n");

  // Delete the remaining text on the line.
  ddrh.m_doc.deleteAt(TextMCoord(0, 0), 3);
  ddrh.checkFile("\n\n");

  // Delete the entire line.
  ddrh.m_doc.deleteAt(TextMCoord(0, 0), 1);
  ddrh.checkFile("\n");

  // Track a new version.
  VersionNumber ver2 = ddrh.m_doc.getVersionNumber();

  // Make diagnostics for the new version and track them.
  {
    TextDocumentDiagnostics diagnostics(ver2);
    diagnostics.insertDiagnostic({{0,0}, {0,0}}, TDD_Diagnostic("msg0"));
    diagnostics.insertDiagnostic({{1,0}, {1,0}}, TDD_Diagnostic("msg1"));
    ddrh.saveVersion(diagnostics);

    EXPECT_EQ(ddrh.m_recorder.isTracking(ver2), true);
  }

  // Insert a few lines.
  ddrh.m_doc.insertAt(TextMCoord(0, 0), "\n\n\n", 3);
  ddrh.checkFile("\n\n\n\n");

  // Check that the eagerly-updated diagnostics are right.
  EXPECT_EQ(toGDValue(ddrh.m_verToDiags.at(ver1).m_eagerDiags), fromGDVN(R"(
    {
      TDD_DocEntry[
        range: MCR(MC(4 0) MC(4 0))
        diagnostic: TDD_Diagnostic[message:"msg" related:[]]
      ]
    }
  )"));
  EXPECT_EQ(toGDValue(ddrh.m_verToDiags.at(ver2).m_eagerDiags), fromGDVN(R"(
    {
      TDD_DocEntry[
        range: MCR(MC(0 0) MC(0 0))
        diagnostic: TDD_Diagnostic[message:"msg0" related:[]]
      ]
      TDD_DocEntry[
        range: MCR(MC(4 0) MC(4 0))
        diagnostic: TDD_Diagnostic[message:"msg1" related:[]]
      ]
    }
  )"));

  // Roll forward version 1, checking it matches the eager diags.
  ddrh.checkSavedVersion(ver1);

  // Same for versin 2.
  ddrh.checkSavedVersion(ver2);
  EXPECT_EQ(ddrh.m_recorder.trackingSomething(), false);
}



CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_td_obs_recorder(CmdlineArgsSpan args)
{
  test_basics();
  test_DDRH();

  // TODO: More!
}


// EOF
