// nearby-file-test.cc
// Test code for 'nearby-file' module.

#include "unit-tests.h"                // decl for my entry point

#include "host-and-resource-name.h"    // HostAndResourceName
#include "host-file-and-line-opt.h"    // HostFileAndLineOpt
#include "nearby-file.h"               // module to test

// smbase
#include "smbase/container-util.h"     // smbase::contains
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional) for EXPECT_EQ_GDVSER
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
  HostFileAndLineOpt const &actual,
  HostAndResourceName const &expectHARN)
{
  // The test infrastructure uses empty names to indicate places we
  // expect to get an absent `HostFileAndLineOpt`.
  EXPECT_EQ(actual.hasFilename(), !expectHARN.empty());

  if (actual.hasFilename()) {
    EXPECT_EQ(actual.getHarn(), expectHARN);
  }
}


static void expectIGNF(
  IHFExists &hfe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostAndResourceName const &expectHARN)
{
  HostFileAndLineOpt actual = getNearbyFilename(hfe,
    candidatePrefixes, haystack, charOffset);
  checkActualHarn(actual, expectHARN);
  EXPECT_EQ_GDVSER(actual.getLineOpt(), std::nullopt);
}


static void expectLocalIGNF(
  IHFExists &hfe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  std::string const &expectLocalFile)
{
  HostAndResourceName expectHARN(
    HostAndResourceName::localFile(expectLocalFile));
  expectIGNF(hfe, candidatePrefixes,
             haystack, charOffset,
             expectHARN);
}


// Class that recognizes a fixed set of host+file pairs.
class TestIHFExists : public IHFExists {
public:      // data
  // Set of recognized names.
  std::set<HostAndResourceName> m_existingHARNs;

public:      // methods
  virtual bool hfExists(HostAndResourceName const &harn) override
  {
    return contains(m_existingHARNs, harn);
  }
};


// Insert some entries for testing.
static void populate(TestIHFExists &hfe)
{
  // Some local paths.
  static char const * const localPaths[] = {
    "/home/foo.txt",
    "/home/user/foo.txt",
    "/home/user/bar.txt",
    "/smbase/sm-test.h",
  };
  for (auto s : localPaths) {
    hfe.m_existingHARNs.insert(HostAndResourceName::localFile(s));
  }

  // Also some remote paths.
  static char const * const remotePaths[] = {
    "/mnt/file1.txt",
    "/mnt/file2.txt",
  };
  HostName remoteHost(HostName::asSSH("host"));
  for (auto s : remotePaths) {
    hfe.m_existingHARNs.insert(HostAndResourceName(remoteHost, s));
  }
}


static void test1()
{
  TestIHFExists hfe;
  populate(hfe);

  std::vector<HostAndResourceName> prefixes;

  // No prefixes.
  expectLocalIGNF(hfe, prefixes, "anything", 0, "");

  prefixes.push_back(HostAndResourceName::localFile("/home"));
  expectLocalIGNF(hfe, prefixes, "foo.txt", 0, "/home/foo.txt");
  expectLocalIGNF(hfe, prefixes, "foo.txt", 6, "/home/foo.txt");
  expectLocalIGNF(hfe, prefixes, "foo.txt", 7, "/home/foo.txt");

  // Empty input line.
  expectLocalIGNF(hfe, prefixes, "", 0, "");

  // Offset out of bounds.
  expectLocalIGNF(hfe, prefixes, "foo.txt", -1, "");
  expectLocalIGNF(hfe, prefixes, "foo.txt", 8, "");

  // No absolute search path yet, but this is the result when nothing
  // found and the start string is absolute, so it's hard to see the
  // effect...
  expectLocalIGNF(hfe, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Now it will work.
  prefixes.push_back(HostAndResourceName::localFile(""));
  expectLocalIGNF(hfe, prefixes, "/home/foo.txt", 3, "/home/foo.txt");

  // Prefix priority.
  expectLocalIGNF(hfe, prefixes, "bar.txt", 0, "/home/bar.txt");       // not found
  prefixes.push_back(HostAndResourceName::localFile("/home/user"));
  expectLocalIGNF(hfe, prefixes, "foo.txt", 0, "/home/foo.txt");       // still
  expectLocalIGNF(hfe, prefixes, "bar.txt", 0, "/home/user/bar.txt");  // now found

  // Range of file name characters.  None exist, that's fine.
  expectLocalIGNF(hfe, prefixes, "ab cd ef", 3,
                  "/home/cd");
  expectLocalIGNF(hfe, prefixes, "ab cd ef", 4,
                  "/home/cd");
  expectLocalIGNF(hfe, prefixes, "ab cd ef", 5,
                  "/home/cd");

  // Test inclusion.
  //
  // 2022-08-19: I do not recall what this was testing!  I think it has
  // to do with ignoring instances where the cursor is on consecutive
  // punctuation characters.
  expectLocalIGNF(hfe, prefixes, "ab cAZaz90_d ef", 7,
                  "/home/cAZaz90_d");
  expectLocalIGNF(hfe, prefixes, "ab z/y\\-__cAZaz90_d ef", 8, "");
  expectLocalIGNF(hfe, prefixes, "ab z/y\\-__cAZaz90_d ef", 11,
                  "/home/z/y/-__cAZaz90_d");

  // Test exclusion.
  expectLocalIGNF(hfe, prefixes, "ab \"cd\" ef", 5,
                  "/home/cd");
  expectLocalIGNF(hfe, prefixes, "ab <cd> ef", 5,
                  "/home/cd");
  expectLocalIGNF(hfe, prefixes, "ab 'cd' ef", 5,
                  "/home/cd");

  // Test that we ignore starting on "//".
  expectLocalIGNF(hfe, prefixes, "// blah", 0, "");
  expectLocalIGNF(hfe, prefixes, "//blah", 0, "");
  expectLocalIGNF(hfe, prefixes, "/blah", 0, "/blah");

  // Ignore trailing punctuation.
  expectLocalIGNF(hfe, prefixes, "foo.txt.", 0, "/home/foo.txt");
  expectLocalIGNF(hfe, prefixes, "foo.txt.", 6, "/home/foo.txt");

  // Test dropping dots.
  expectLocalIGNF(hfe, prefixes, "./foo.txt", 1, "/home/foo.txt");
  expectLocalIGNF(hfe, prefixes, "./a/../foo.txt", 1, "/home/foo.txt");

  // Test that we drop dots even when we cannot confirm the file exists.
  expectLocalIGNF(hfe, prefixes, "./a/../goo.txt", 1, "/home/goo.txt");
}


static void expectIGNFL(
  IHFExists &hfe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostAndResourceName const &expectHARN,
  int intExpectLine)     // Hence the final "L" in this function's name.
{
  HostFileAndLineOpt actual = getNearbyFilename(hfe,
    candidatePrefixes, haystack, charOffset);
  checkActualHarn(actual, expectHARN);

  // Make a properly typed `expectLine` from the integer code used in
  // the tests.
  std::optional<LineNumber> expectLine =
    intExpectLine?
      std::make_optional<LineNumber>(intExpectLine) :
      std::nullopt;

  EXPECT_EQ_GDVSER(actual.getLineOpt(), expectLine);
}


static void expectLocalIGNFL(
  IHFExists &hfe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  std::string const &expectName,
  int expectLine)
{
  HostAndResourceName expectHARN(
    HostAndResourceName::localFile(expectName));
  expectIGNFL(hfe, candidatePrefixes,
              haystack, charOffset,
              expectHARN, expectLine);
}


static void expectRemoteIGNFL(
  IHFExists &hfe,
  std::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  int charOffset,
  HostName const &expectHostName,
  std::string const &expectName,
  int expectLine)
{
  HostAndResourceName expectHARN(expectHostName, expectName);
  expectIGNFL(hfe, candidatePrefixes,
              haystack, charOffset,
              expectHARN, expectLine);
}


static void testLineNumbers()
{
  TestIHFExists hfe;
  populate(hfe);

  std::vector<HostAndResourceName> prefixes;

  // No prefixes.
  expectLocalIGNFL(hfe, prefixes, "anything:1", 0, "", 0);

  // Limits on where the search can begin.
  prefixes.push_back(HostAndResourceName::localFile("/home"));
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", -1, "", 0);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 0, "/home/foo.txt", 3);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 6, "/home/foo.txt", 3);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 7, "/home/foo.txt", 3);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 8, "", 0);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 9, "", 0);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3", 10, "", 0);

  // Maximum of 9 digits.
  expectLocalIGNFL(hfe, prefixes, "foo.txt:123456789", 0, "/home/foo.txt", 123456789);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:1234567890", 0, "/home/foo.txt", 0);

  // Line number can't run straight into letters.
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3a", 0, "/home/foo.txt", 0);
  expectLocalIGNFL(hfe, prefixes, "foo.txt:3 a", 0, "/home/foo.txt", 3);

  // Report best match even for non-existent, including line number.
  expectLocalIGNFL(hfe, prefixes, "baz.txt:3: something", 0, "/home/baz.txt", 3);

  // Find a file starting with "./".
  expectLocalIGNFL(hfe, prefixes, "./foo.txt:3", 0, "/home/foo.txt", 3);

  // And "../".
  expectLocalIGNFL(hfe, prefixes, "../smbase/sm-test.h:3", 0, "/smbase/sm-test.h", 3);
}


static void testRemoteFiles()
{
  TestIHFExists hfe;
  populate(hfe);

  std::vector<HostAndResourceName> prefixes;
  prefixes.push_back(HostAndResourceName::localFile("/home"));

  // Look for a file that exists remotely but isn't in 'prefixes' yet.
  // The returned name in this case is due to the fallback behavior.
  expectLocalIGNFL(hfe, prefixes, "file1.txt:3", 0, "/home/file1.txt", 3);

  // Now add the remote prefix.
  HostName host(HostName::asSSH("host"));
  prefixes.push_back(HostAndResourceName(host, "/mnt"));

  // Should find the file.
  expectRemoteIGNFL(hfe, prefixes, "file1.txt:3", 0, host, "/mnt/file1.txt", 3);
}


// Called from unit-tests.cc.
void test_nearby_file(CmdlineArgsSpan args)
{
  test1();
  testLineNumbers();
  testRemoteFiles();
}


// EOF
