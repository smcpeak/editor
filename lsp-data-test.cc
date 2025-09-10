// lsp-data-test.cc
// Tests for `lsp-data` and `lsp-conv` modules.

#include "unit-tests.h"                // decl for my entry point

#include "lsp-conv.h"                  // module under test
#include "lsp-data.h"                  // module under test

#include "named-td.h"                  // NamedTextDocument
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "uri-util.h"                  // getFileURIPath

#include "smbase/gdvalue-json.h"       // gdv::gdvToJSON
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // TEST_CASE

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// Populate `doc` with `numLines`, each with `numCols`.
//
// This is just to have a place for the locations in the primary
// diagnostic message to be associated with, since they require valid
// model coordinates.
void populateNTD(NamedTextDocument &doc, int numLines, int numCols)
{
  for (int line=0; line < numLines; ++line) {
    std::string text(numCols, 'x');
    doc.appendString(text + "\n");
  }
}


// Convert `lspPDP` to `TextDocumentDiagnostics`, then convert that to
// GDVN and compare to `expectGDVN`.
void convertToTDDExpect(
  LSP_PublishDiagnosticsParams const &lspPDP,
  char const *expectGDVN)
{
  // Make a document for the diagnostics to follow.
  NamedTextDocument doc;
  populateNTD(doc, 10, 10);

  // Convert to TDD.
  std::unique_ptr<TextDocumentDiagnostics> tdd =
    convertLSPDiagsToTDD(&lspPDP, URIPathSemantics::NORMAL);
  tdd->adjustForDocument(doc.getCore());

  // Render that as GDValue.
  GDValue tddGDV = toGDValue(*tdd);

  // Compare to expectation.
  GDValueWriteOptions opts;
  opts.m_indentLevel = 1;
  EXPECT_EQ(tddGDV.asIndentedString(opts), expectGDVN);
}


// Do a round-trip serialization test with a simple example.
void test_PublishDiagnosticsParams_simple()
{
  TEST_CASE("test_PublishDiagnosticsParams_simple");

  GDValue v(GDVMap{
    { "uri", "file:///D:/home/User/foo.cc" },
    { "version", 3 },
    { "diagnostics", GDVSequence{
      GDVMap{
        { "range", GDVMap{
          { "start", GDVMap{
            { "line", 4 },
            { "character", 5 },
          }},
          { "end", GDVMap{
            { "line", 6 },
            { "character", 7 },
          }},
        }},
        { "severity", 2 },
        { "message", "primary message" },
        // No related information here.
      }},
    },
  });
  DIAG(gdvToJSON(v, GDValueWriteOptions().setEnableIndentation(true)));

  LSP_PublishDiagnosticsParams lspPDP{GDValueParser(v)};
  GDValue v2 = lspPDP;

  GDValue &firstDiag =
    v2.mapGetValueAt("diagnostics").sequenceGetValueAt(0);

  // We normalize the absence of related information into an empty
  // sequence.
  EXPECT_EQ(firstDiag.mapGetValueAt("relatedInformation"),
            GDValue(GDVSequence{}));

  // To aid the comparison, remove it now.
  firstDiag.mapRemoveKey("relatedInformation");

  // Similarly, an absent source is represented as a null source.
  EXPECT_EQ(firstDiag.mapGetValueAt("source"), GDValue());
  firstDiag.mapRemoveKey("source");

  // Then the two should be equal.
  EXPECT_EQ(v2, v);


  convertToTDDExpect(lspPDP, R"({
    TDD_DocEntry[
      range: MCR(MC(4 5) MC(6 7))
      diagnostic: TDD_Diagnostic[message:"primary message" related:[]]
    ]
  })");
}


// Add some "relatedInformation".
void test_PublishDiagnosticsParams_withRelated()
{
  TEST_CASE("test_PublishDiagnosticsParams_withRelated");

  GDValue v(GDVMap{
    { "uri", "file:///D:/home/User/foo.cc" },
    { "version", 3 },
    { "diagnostics", GDVSequence{
      GDVMap{
        { "range", GDVMap{
          { "start", GDVMap{
            { "line", 4 },
            { "character", 5 },
          }},
          { "end", GDVMap{
            { "line", 6 },
            { "character", 7 },
          }},
        }},
        { "severity", 2 },
        { "message", "primary message" },
        { "source", GDValue() },
        { "relatedInformation", GDVSequence{
          GDVMap{
            { "location", GDVMap{
              { "uri", "file:///D:/home/User/other.h" },
              { "range", GDVMap{
                { "start", GDVMap{
                  { "line", 14 },
                  { "character", 15 },
                }},
                { "end", GDVMap{
                  { "line", 16 },
                  { "character", 17 },
                }},
              }},
            }},
            { "message", "aux message 1" },
          },
          GDVMap{
            { "location", GDVMap{
              { "uri", "file:///D:/home/User/other.h" },
              { "range", GDVMap{
                { "start", GDVMap{
                  { "line", 114 },
                  { "character", 115 },
                }},
                { "end", GDVMap{
                  { "line", 116 },
                  { "character", 117 },
                }},
              }},
            }},
            { "message", "aux message 2" },
          },
        }},
      }},
    },
  });
  DIAG(gdvToJSON(v, GDValueWriteOptions().setEnableIndentation(true)));

  LSP_PublishDiagnosticsParams pdp{GDValueParser(v)};
  GDValue v2 = pdp;

  EXPECT_EQ(v2, v);

  convertToTDDExpect(pdp, R"({
    TDD_DocEntry[
      range: MCR(MC(4 5) MC(6 7))
      diagnostic: TDD_Diagnostic[
        message: "primary message"
        related: [
          TDD_Related[
            file: "D:/home/User/other.h"
            lineIndex: 14
            message: "aux message 1"
          ]
          TDD_Related[
            file: "D:/home/User/other.h"
            lineIndex: 114
            message: "aux message 2"
          ]
        ]
      ]
    ]
  })");
}


void test_LocationSequence1()
{
  LSP_LocationSequence seq(GDValueParser(fromGDVN(R"(
    [
      {
        "range": {
          "end": {"character":8 "line":18}
          "start": {"character":5 "line":18}
        }
        "uri":
          "file:///D:/cygwin/home/Scott/wrk/editor/test/language-test.cc"
      }
    ]
  )")));

  EXPECT_EQ(seq.m_locations.size(), 1);

  LSP_Location const &loc = seq.m_locations.front();
  EXPECT_EQ(loc.getFname(URIPathSemantics::NORMAL),
    "D:/cygwin/home/Scott/wrk/editor/test/language-test.cc");
  EXPECT_EQ(loc.m_range,
    LSP_Range(LSP_Position(LineIndex(18), ByteIndex(5)),
              LSP_Position(LineIndex(18), ByteIndex(8))));
}


void test_LocationSequence2()
{
  LSP_LocationSequence seq(GDValueParser(fromGDVN(R"(
    [
      {
        "range": {
          "end": {"character":8 "line":18}
          "start": {"character":5 "line":18}
        }
        "uri":
          "file:///D:/cygwin/home/Scott/wrk/editor/test/language-test.cc"
      }
      {
        "range": {
          "end": {"character":18 "line":118}
          "start": {"character":15 "line":118}
        }
        "uri":
          "file:///D:/cygwin/home/Scott/wrk/editor/test/language-test.cc"
      }
    ]
  )")));

  EXPECT_EQ(seq.m_locations.size(), 2);

  auto it = seq.m_locations.begin();
  {
    LSP_Location const &loc = *it;
    EXPECT_EQ(loc.getFname(URIPathSemantics::NORMAL),
      "D:/cygwin/home/Scott/wrk/editor/test/language-test.cc");
    EXPECT_EQ(loc.m_range,
      LSP_Range(LSP_Position(LineIndex(18), ByteIndex(5)),
                LSP_Position(LineIndex(18), ByteIndex(8))));
  }

  ++it;
  {
    LSP_Location const &loc = *it;
    EXPECT_EQ(loc.getFname(URIPathSemantics::NORMAL),
      "D:/cygwin/home/Scott/wrk/editor/test/language-test.cc");
    EXPECT_EQ(loc.m_range,
      LSP_Range(LSP_Position(LineIndex(118), ByteIndex(15)),
                LSP_Position(LineIndex(118), ByteIndex(18))));
  }
}


CLOSE_ANONYMOUS_NAMESPACE


void test_lsp_data(CmdlineArgsSpan args)
{
  test_PublishDiagnosticsParams_simple();
  test_PublishDiagnosticsParams_withRelated();
  test_LocationSequence1();
  test_LocationSequence2();
}


// EOF
