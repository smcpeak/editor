// host-file-and-line-opt-test.cc
// Tests for `host-file-and-line-opt` module.

#include "unit-tests.h"                // decl for my entry point
#include "host-file-and-line-opt.h"    // module under test

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// As this class is primarily just a passive container, exercising
// serialization covers a lot of its functionality.
void test_gdvn()
{
  EXPECT_EQ(toGDValue(HostFileAndLineOpt()).asString(),
    "HostFileAndLineOpt[harn:null line:null byteIndex:null]");

  HostAndResourceName harn(HostName::asSSH("host"), "resName");
  HostFileAndLineOpt hfal(harn, LineNumber(3), ByteIndex(4));
  EXPECT_EQ(toGDValue(hfal).asString(),
    "HostFileAndLineOpt["
      "harn:HostAndResourceName["
        "hostName:HostName[sshHostName:\"host\"] "
        "resourceName:\"resName\""
      "] "
      "line:3 "
      "byteIndex:4"
    "]");
}


CLOSE_ANONYMOUS_NAMESPACE


void test_host_file_and_line_opt(CmdlineArgsSpan args)
{
  test_gdvn();
}


// EOF
