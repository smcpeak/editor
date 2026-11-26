// ui-shell-script-file.cc
// Code for `ui-shell-script-file` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-script-file.h"                // this module

// smbase
#include "smbase/gdvalue-fnapply-transform.h"    // gdv::GDValueFnApplyTransform
#include "smbase/gdvalue-parser.h"               // gdv::{GDValueParser, gdvpOptTo}
#include "smbase/gdvalue-vector.h"               // gdv::gdvpTo(std::vector)
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-script-file");


// Copy the elements of `src` into `dest` while flattening all sequences
// immediately nested in it.  `null` is treated like an empty sequence,
// and hence skipped.  Tagged sequences are not affected.
static void gdvFlattenSequence(
  std::vector<GDValue> &dest /*APPEND*/,
  GDValue const &src)
{
  if (src.isSequence() && !src.isTaggedContainer()) {
    for (GDValue const &elt : src.sequenceIterableC()) {
      gdvFlattenSequence(dest, elt);
    }
  }
  else if (src.isNull()) {
    // Drop nulls.
  }
  else {
    dest.push_back(src);
  }
}


UIShellScriptFile::UIShellScriptFile()
  : m_args(),
    m_commands()
{}


UIShellScriptFile::UIShellScriptFile(std::string const &fname)
  : UIShellScriptFile()
{
  // Read the script and parse it.
  GDValue testScript = GDValue::readFromFile(fname);
  GDValueParser parser(testScript);
  parser.checkIsMap();

  // The `args` can be missing, in which case it is empty.
  m_args = gdvpOptTo<std::vector<std::string>>(
    parser.mapGetValueAtSymOpt("args"));

  // Build the transform object so we can populate it with definitions.
  GDValueFnApplyTransform transform;

  SMFileUtil sfu;

  // Optional `includes` to read first for definitions.
  auto includes = gdvpOptTo<std::vector<std::string>>(
    parser.mapGetValueAtSymOpt("includes"));
  for (std::string const &include : includes) {
    // Treat the name as relative to where `fname` is.
    std::string joinedInclude =
      sfu.joinIfRelativeFilename(
        sfu.splitPathDir(fname),
        include);

    // Add the definitions to `transform`.  The result of processing
    // them is discarded.
    TRACE1("processing include: " << joinedInclude);
    transform.transform(GDValue::readFromFile(joinedInclude));
  }

  // The `cmds` is required.
  GDValueParser origCmds = parser.mapGetValueAtSym("cmds");

  // Evaluate and substitute definitions.
  GDValue substCmds = transform.transform(origCmds.getValue());
  TRACE3("substCmds dump: " << substCmds.dumpToString());

  // Flatten as a sequence of values.
  gdvFlattenSequence(m_commands /*APPEND*/, substCmds);
}


// EOF
