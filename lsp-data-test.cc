// lsp-data-test.cc
// Tests for `lsp-data` module.

#include "lsp-data.h"                  // module under test

#include "smbase/gdvalue-json.h"       // gdv::gdvToJSON
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // TEST_CASE

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


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

  LSP_PublishDiagnosticsParams pdp{GDValueParser(v)};
  GDValue v2 = pdp;

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
}


CLOSE_ANONYMOUS_NAMESPACE


void test_lsp_data()
{
  test_PublishDiagnosticsParams_simple();
  test_PublishDiagnosticsParams_withRelated();
}


// EOF
