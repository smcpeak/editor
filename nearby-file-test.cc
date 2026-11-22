// nearby-file-test.cc
// Test code for 'nearby-file' module.

#include "unit-tests.h"                // decl for my entry point
#include "nearby-file.h"               // module to test

// editor
#include "host-and-resource-name.h"    // HostAndResourceName
#include "host-file-olb.h"             // HostFile_OptLineByte
#include "line-number.h"               // LineNumber
#include "mock-host-file-probe.h"      // HostFileProbe, MockHostFileProbe

// smbase
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional) for EXPECT_EQ_GDVSER
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::vector) for TEST_FUNC_EXPRS
#include "smbase/sm-file-util.h"       // TestSMFileUtil
#include "smbase/sm-test.h"            // USUAL_TEST_MAIN

// libc++
#include <optional>                    // std::{make_optional, nullopt, optional}
#include <set>                         // std::set
#include <string>                      // std::string
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


// Check that the "harn" element of `actual` is what we expect.
static void checkActualHarn(
  std::optional<HostFile_OptLineByte> const &actual,
  HostAndResourceName const &expectHARN)
{
  // The test infrastructure uses empty names to indicate places we
  // expect to get an absent `HostFile_OptLineByte`.
  EXPECT_EQ(actual.has_value(), !expectHARN.empty());

  if (actual) {
    EXPECT_EQ(actual->getHarn(), expectHARN);
  }
}


// Check that the `m_line` element of `actual` is what we expect.
static void checkActualLine(
  std::optional<HostFile_OptLineByte> const &actual,
  std::optional<LineNumber> expectLineNumber)
{
  if (expectLineNumber) {
    EXPECT_TRUE(actual.has_value());
  }

  if (actual.has_value()) {
    std::optional<LineIndex> actualLineIndexOpt =
      actual->getLineIndexOpt();

    EXPECT_EQ(actualLineIndexOpt.has_value(),
              expectLineNumber.has_value());

    if (expectLineNumber.has_value()) {
      // The tests are written using line *numbers*, but the internal
      // interface uses line *indices*.
      EXPECT_EQ(actualLineIndexOpt->toLineNumber(),
                *expectLineNumber);
    }
  }
}


static void expectIGNF(
  HostFileProbe &probe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostAndResourceName const &expectHARN)
{
  std::optional<HostFile_OptLineByte> actual = getNearbyFilename(probe,
    candidatePrefixes, haystack, ByteIndex(charOffset));
  checkActualHarn(actual, expectHARN);
  checkActualLine(actual, std::nullopt);
}


static void expectLocalIGNF(
  HostFileProbe &probe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  std::string const &expectLocalFile)
{
  HostAndResourceName expectHARN(
    HostAndResourceName::localFile(expectLocalFile));
  expectIGNF(probe, candidatePrefixes,
             haystack, charOffset,
             expectHARN);
}


// Insert some entries for testing.
static void populate(MockHostFileProbe &probe)
{
  // Some local paths.
  static char const * const localPaths[] = {
    "/home/foo.txt",
    "/home/user/foo.txt",
    "/home/user/bar.txt",
    "/smbase/sm-test.h",
  };
  for (auto s : localPaths) {
    probe.add(HostAndResourceName::localFile(s));
  }

  // Also some remote paths.
  static char const * const remotePaths[] = {
    "/mnt/file1.txt",
    "/mnt/file2.txt",
  };
  HostName remoteHost(HostName::asSSH("host"));
  for (auto s : remotePaths) {
    probe.add(HostAndResourceName(remoteHost, s));
  }
}


static void test1()
{
  MockHostFileProbe probe;
  populate(probe);

  std::vector<HostAndResourceName> prefixes;

  // No prefixes.
  expectLocalIGNF(probe, prefixes, "anything", 0, "");

  prefixes.push_back(HostAndResourceName::localFile("/home"));
  expectLocalIGNF(probe, prefixes, "foo.txt", 0, "/home/foo.txt");
  expectLocalIGNF(probe, prefixes, "foo.txt", 6, "/home/foo.txt");
  expectLocalIGNF(probe, prefixes, "foo.txt", 7, "/home/foo.txt");

  // Empty input line.
  expectLocalIGNF(probe, prefixes, "", 0, "");

  // Offset out of bounds.
  expectLocalIGNF(probe, prefixes, "foo.txt", 8, "");

  // No absolute search path yet, but this is the result when nothing
  // found and the start string is absolute, so it's hard to see the
  // effect...
  expectLocalIGNF(probe, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Now it will work.
  prefixes.push_back(HostAndResourceName::localFile(""));
  expectLocalIGNF(probe, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Prefix priority.
  expectLocalIGNF(probe, prefixes, "bar.txt", 0, "/home/bar.txt");       // not found
  prefixes.push_back(HostAndResourceName::localFile("/home/user"));
  expectLocalIGNF(probe, prefixes, "foo.txt", 0, "/home/foo.txt");       // still
  expectLocalIGNF(probe, prefixes, "bar.txt", 0, "/home/user/bar.txt");  // now found

  // Range of file name characters.  None exist, that's fine.
  expectLocalIGNF(probe, prefixes, "ab cd ef", 3,
                  "/home/cd");
  expectLocalIGNF(probe, prefixes, "ab cd ef", 4,
                  "/home/cd");
  expectLocalIGNF(probe, prefixes, "ab cd ef", 5,
                  "/home/cd");

  // Test inclusion.
  //
  // 2022-08-19: I do not recall what this was testing!  I think it has
  // to do with ignoring instances where the cursor is on consecutive
  // punctuation characters.
  expectLocalIGNF(probe, prefixes, "ab cAZaz90_d ef", 7,
                  "/home/cAZaz90_d");
  expectLocalIGNF(probe, prefixes, "ab z/y\\-__cAZaz90_d ef", 8, "");
  expectLocalIGNF(probe, prefixes, "ab z/y\\-__cAZaz90_d ef", 11,
                  "/home/z/y/-__cAZaz90_d");

  // Test exclusion.
  expectLocalIGNF(probe, prefixes, "ab \"cd\" ef", 5,
                  "/home/cd");
  expectLocalIGNF(probe, prefixes, "ab <cd> ef", 5,
                  "/home/cd");
  expectLocalIGNF(probe, prefixes, "ab 'cd' ef", 5,
                  "/home/cd");

  // Test that we ignore starting on "//".
  expectLocalIGNF(probe, prefixes, "// blah", 0, "");
  expectLocalIGNF(probe, prefixes, "//blah", 0, "");
  expectLocalIGNF(probe, prefixes, "/blah", 0, "/blah");

  // Ignore trailing punctuation.
  expectLocalIGNF(probe, prefixes, "foo.txt.", 0, "/home/foo.txt");
  expectLocalIGNF(probe, prefixes, "foo.txt.", 6, "/home/foo.txt");

  // Test dropping dots.
  expectLocalIGNF(probe, prefixes, "./foo.txt", 1, "/home/foo.txt");
  expectLocalIGNF(probe, prefixes, "./a/../foo.txt", 1, "/home/foo.txt");

  // Test that we drop dots even when we cannot confirm the file exists.
  expectLocalIGNF(probe, prefixes, "./a/../goo.txt", 1, "/home/goo.txt");
}


static void expectIGNFL(
  HostFileProbe &probe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostAndResourceName const &expectHARN,
  int intExpectLineNumber)     // Hence the final "L" in this function's name.
{
  TEST_FUNC_EXPRS(candidatePrefixes, haystack, charOffset);

  std::optional<HostFile_OptLineByte> actual = getNearbyFilename(probe,
    candidatePrefixes, haystack, ByteIndex(charOffset));
  checkActualHarn(actual, expectHARN);

  // Make a properly typed `expectLineNumber` from the integer code used
  // in the tests.
  std::optional<LineNumber> expectLineNumber =
    intExpectLineNumber?
      std::make_optional<LineNumber>(intExpectLineNumber) :
      std::nullopt;
  checkActualLine(actual, expectLineNumber);
}


static void expectLocalIGNFL(
  HostFileProbe &probe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  std::string const &expectName,
  int expectLine)
{
  HostAndResourceName expectHARN(
    HostAndResourceName::localFile(expectName));
  expectIGNFL(probe, candidatePrefixes,
              haystack, charOffset,
              expectHARN, expectLine);
}


static void expectRemoteIGNFL(
  HostFileProbe &probe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostName const &expectHostName,
  std::string const &expectName,
  int expectLine)
{
  HostAndResourceName expectHARN(expectHostName, expectName);
  expectIGNFL(probe, candidatePrefixes,
              haystack, charOffset,
              expectHARN, expectLine);
}


static void testLineNumbers()
{
  MockHostFileProbe probe;
  populate(probe);

  std::vector<HostAndResourceName> prefixes;

  // No prefixes.
  expectLocalIGNFL(probe, prefixes, "anything:1", 0, "", 0);

  // Limits on where the search can begin.
  prefixes.push_back(HostAndResourceName::localFile("/home"));
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 0, "/home/foo.txt", 3);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 6, "/home/foo.txt", 3);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 7, "/home/foo.txt", 3);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 8, "", 0);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 9, "", 0);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3", 10, "", 0);

  // Maximum of 9 digits.
  expectLocalIGNFL(probe, prefixes, "foo.txt:123456789", 0, "/home/foo.txt", 123456789);
  expectLocalIGNFL(probe, prefixes, "foo.txt:1234567890", 0, "/home/foo.txt", 0);

  // Line number can't run straight into letters.
  expectLocalIGNFL(probe, prefixes, "foo.txt:3a", 0, "/home/foo.txt", 0);
  expectLocalIGNFL(probe, prefixes, "foo.txt:3 a", 0, "/home/foo.txt", 3);

  // Report best match even for non-existent, including line number.
  expectLocalIGNFL(probe, prefixes, "baz.txt:3: something", 0, "/home/baz.txt", 3);

  // Find a file starting with "./".
  expectLocalIGNFL(probe, prefixes, "./foo.txt:3", 0, "/home/foo.txt", 3);

  // And "../".
  expectLocalIGNFL(probe, prefixes, "../smbase/sm-test.h:3", 0, "/smbase/sm-test.h", 3);
}


static void testRemoteFiles()
{
  MockHostFileProbe probe;
  populate(probe);

  std::vector<HostAndResourceName> prefixes;
  prefixes.push_back(HostAndResourceName::localFile("/home"));

  // Look for a file that exists remotely but isn't in 'prefixes' yet.
  // The returned name in this case is due to the fallback behavior.
  expectLocalIGNFL(probe, prefixes, "file1.txt:3", 0, "/home/file1.txt", 3);

  // Now add the remote prefix.
  HostName host(HostName::asSSH("host"));
  prefixes.push_back(HostAndResourceName(host, "/mnt"));

  // Should find the file.
  expectRemoteIGNFL(probe, prefixes, "file1.txt:3", 0, host, "/mnt/file1.txt", 3);
}


// Called from unit-tests.cc.
void test_nearby_file(CmdlineArgsSpan args)
{
  test1();
  testLineNumbers();
  testRemoteFiles();
}


// EOF
