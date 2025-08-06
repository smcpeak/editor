// editor-settings.cc
// Code for editor-settings.h.

#include "editor-settings.h"           // this module

#include "editor-command.ast.gen.h"    // EditorCommand

// Ensure the required templates are all declared before use.
#include "smbase/gdvalue-map-fwd.h"
#include "smbase/gdvalue-set-fwd.h"
#include "smbase/gdvalue-unique-ptr-fwd.h"
#include "smbase/gdvalue-vector-fwd.h"

#include "smbase/container-util.h"     // smbase::contains
#include "smbase/gdvalue-map.h"        // GDValue <-> std::map
#include "smbase/gdvalue-parser-ops.h" // gdv::GDValueParser
#include "smbase/gdvalue-set.h"        // GDValue <-> std::set
#include "smbase/gdvalue-unique-ptr.h" // GDValue <-> std::unique_ptr
#include "smbase/gdvalue-vector.h"     // GDValue <-> std::vector
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // keySet
#include "smbase/set-util.h"           // smbase::setInsert
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <utility>                     // std::swap


using namespace gdv;
using namespace smbase;


INIT_TRACE("editor-settings");


// Version number for the settings file format.
//
// My current thinking is I will only bump this when necessary to
// prevent misinterpretation, which ideally would be never.  I should
// mostly be able to just add fields.
//
static int const CUR_VERSION = 1;


// ------------------------ CommandLineHistory -------------------------
CommandLineHistory::~CommandLineHistory()
{}


CommandLineHistory::CommandLineHistory()
  : m_commands(),
    m_recent(),
    m_useSubstitution(true),
    m_prefixStderrLines(false)
{}


CommandLineHistory::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "CommandLineHistory"_sym);

  GDV_WRITE_MEMBER_SYM(m_commands);
  GDV_WRITE_MEMBER_SYM(m_recent);
  GDV_WRITE_MEMBER_SYM(m_useSubstitution);
  GDV_WRITE_MEMBER_SYM(m_prefixStderrLines);

  return m;
}


CommandLineHistory::CommandLineHistory(gdv::GDValueParser const &p)
  : GDVP_READ_OPT_MEMBER_SYM(m_commands),
    GDVP_READ_OPT_MEMBER_SYM(m_recent),
    GDVP_READ_OPT_MEMBER_SYM(m_useSubstitution),
    GDVP_READ_OPT_MEMBER_SYM(m_prefixStderrLines)
{
  p.checkTaggedOrderedMapTag("CommandLineHistory");
}


// Swap `this->memb` with `obj.memb`.  This should be done after
// `using std::swap;`.
//
// This is a candidate to move someplace more general, but it is for now
// experimental.
#define SWAP_MEMB(memb) \
  swap(memb, obj.memb)


void CommandLineHistory::swap(CommandLineHistory &obj)
{
  if (this != &obj) {
    using std::swap;

    SWAP_MEMB(m_commands);
    SWAP_MEMB(m_recent);
    SWAP_MEMB(m_useSubstitution);
    SWAP_MEMB(m_prefixStderrLines);
  }
}


// Set `dest` to `src`, returning true if a change was made.
//
// This is a candidate to move to someplace more general.
template <typename T>
bool setIfDifferent(T &dest, T const &src)
{
  if (dest == src) {
    return false;
  }
  else {
    dest = src;
    return true;
  }
}


bool CommandLineHistory::add(
  std::string const &cmd,
  bool useSubstitution,
  bool prefix)
{
  bool ret = false;

  ret |= setInsert(m_commands, cmd);

  ret |= setIfDifferent(m_recent, cmd);

  ret |= setIfDifferent(m_useSubstitution, useSubstitution);

  ret |= setIfDifferent(m_prefixStderrLines, prefix);

  return ret;
}


bool CommandLineHistory::remove(std::string const &cmd)
{
  bool ret = false;

  ret |= setErase(m_commands, cmd);

  if (cmd == m_recent) {
    ret |= true;
    m_recent.clear();
  }

  return ret;
}


// -------------------------- WindowPosition ---------------------------
WindowPosition::WindowPosition()
  : m_left(0),
    m_top(0),
    m_width(0),
    m_height(0)
{}


WindowPosition::WindowPosition(int left, int top, int width, int height)
  : IMEMBFP(left),
    IMEMBFP(top),
    IMEMBFP(width),
    IMEMBFP(height)
{}


WindowPosition::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "WindowPosition"_sym);
  GDV_WRITE_MEMBER_SYM(m_left);
  GDV_WRITE_MEMBER_SYM(m_top);
  GDV_WRITE_MEMBER_SYM(m_width);
  GDV_WRITE_MEMBER_SYM(m_height);
  return m;
}


WindowPosition::WindowPosition(gdv::GDValueParser const &p)
  : GDVP_READ_OPT_MEMBER_SYM(m_left),
    GDVP_READ_OPT_MEMBER_SYM(m_top),
    GDVP_READ_OPT_MEMBER_SYM(m_width),
    GDVP_READ_OPT_MEMBER_SYM(m_height)
{
  p.checkTaggedOrderedMapTag("WindowPosition");
}


void WindowPosition::swap(WindowPosition &obj)
{
  if (this != &obj) {
    using std::swap;

    SWAP_MEMB(m_left);
    SWAP_MEMB(m_top);
    SWAP_MEMB(m_width);
    SWAP_MEMB(m_height);
  }
}


// -------------------------- EditorSettings ---------------------------
EditorSettings::~EditorSettings()
{}


EditorSettings::EditorSettings()
  : m_macros(),
    m_mostRecentlyRunMacro(),
    m_applyHistory(),
    m_runHistory(),
    m_leftWindowPos(),
    m_rightWindowPos(),
    m_grepsrcSearchesSubrepos(false)
{}


EditorSettings::EditorSettings(GDValueParser const &p)
  : GDVP_READ_OPT_MEMBER_SYM(m_macros),
    GDVP_READ_OPT_MEMBER_SYM(m_mostRecentlyRunMacro),
    GDVP_READ_OPT_MEMBER_SYM(m_applyHistory),
    GDVP_READ_OPT_MEMBER_SYM(m_runHistory),
    GDVP_READ_OPT_MEMBER_SYM(m_leftWindowPos),
    GDVP_READ_OPT_MEMBER_SYM(m_rightWindowPos),
    GDVP_READ_OPT_MEMBER_SYM(m_grepsrcSearchesSubrepos)
{
  p.checkTaggedOrderedMapTag("EditorSettings");

  int version = gdvpTo<int>(p.mapGetValueAtSym("version"));
  if (version > CUR_VERSION) {
    xformatsb("Settings file has version " << version <<
              " but the largest this program can read is " <<
              CUR_VERSION << ".");
  }

  TRACE1("Loaded settings: " << toGDValue(*this));
}


EditorSettings::operator GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "EditorSettings"_sym);

  m.mapSetValueAtSym("version", CUR_VERSION);

  GDV_WRITE_MEMBER_SYM(m_macros);
  GDV_WRITE_MEMBER_SYM(m_mostRecentlyRunMacro);
  GDV_WRITE_MEMBER_SYM(m_applyHistory);
  GDV_WRITE_MEMBER_SYM(m_runHistory);
  GDV_WRITE_MEMBER_SYM(m_leftWindowPos);
  GDV_WRITE_MEMBER_SYM(m_rightWindowPos);
  GDV_WRITE_MEMBER_SYM(m_grepsrcSearchesSubrepos);

  return m;
}


void EditorSettings::swap(EditorSettings &obj)
{
  if (this != &obj) {
    using std::swap;

    SWAP_MEMB(m_macros);
    SWAP_MEMB(m_mostRecentlyRunMacro);
    SWAP_MEMB(m_applyHistory);
    SWAP_MEMB(m_runHistory);
    SWAP_MEMB(m_leftWindowPos);
    SWAP_MEMB(m_rightWindowPos);
    SWAP_MEMB(m_grepsrcSearchesSubrepos);
  }
}


// ------------------------------ macros -------------------------------
static EditorCommandVector cloneECV(EditorCommandVector const &src)
{
  EditorCommandVector dest;

  for (auto const &srcElt : src) {
    dest.push_back(std::unique_ptr<EditorCommand>(srcElt->clone()));
  }

  return dest;
}


void EditorSettings::addMacro(
  std::string const &name,
  EditorCommandVector const &commands)
{
  xassert(!name.empty());
  xassert(!commands.empty());

  EditorCommandVector &dest = m_macros[name];

  // Make sure we're not trying to clone the vector we are also
  // overwriting.
  xassert(&dest != &commands);

  dest = cloneECV(commands);
}


bool EditorSettings::deleteMacro(std::string const &name)
{
  return m_macros.erase(name) > 0;
}


std::set<std::string> EditorSettings::getMacroNames() const
{
  return mapKeySet(m_macros);
}


EditorCommandVector EditorSettings::getMacro(std::string const &name) const
{
  auto it = m_macros.find(name);
  if (it != m_macros.end()) {
    return cloneECV((*it).second);
  }
  else {
    return EditorCommandVector();
  }
}


void EditorSettings::setMostRecentlyRunMacro(std::string const &name)
{
  if (contains(m_macros, name)) {
    m_mostRecentlyRunMacro = name;
  }
  else {
    m_mostRecentlyRunMacro.clear();
  }
}


std::string EditorSettings::getMostRecentlyRunMacro()
{
  if (contains(m_macros, m_mostRecentlyRunMacro)) {
    return m_mostRecentlyRunMacro;
  }
  else {
    TRACE2("Macro name " << toGDValue(m_mostRecentlyRunMacro) <<
           " not among macro keys: " << toGDValue(mapKeySet(m_macros)));

    m_mostRecentlyRunMacro.clear();
    return {};
  }
}


// ----------------------------- commands ------------------------------
CommandLineHistory &EditorSettings::getCommandHistory(
  EditorCommandLineFunction whichFunction)
{
  return const_cast<CommandLineHistory&>(
    getCommandHistoryC(whichFunction));
}


CommandLineHistory const &EditorSettings::getCommandHistoryC(
  EditorCommandLineFunction whichFunction) const
{
  return whichFunction==ECLF_APPLY?
           m_applyHistory :
           m_runHistory;
}


bool EditorSettings::addHistoryCommand(
  EditorCommandLineFunction whichFunction,
  std::string const &cmd,
  bool useSubstitution,
  bool prefixStderrLines)
{
  return getCommandHistory(whichFunction).
    add(cmd, useSubstitution, prefixStderrLines);
}


bool EditorSettings::removeHistoryCommand(
  EditorCommandLineFunction whichFunction,
  std::string const &cmd)
{
  return getCommandHistory(whichFunction).remove(cmd);
}


// ------------------------- window positions --------------------------
void EditorSettings::setLeftWindowPos(WindowPosition const &pos)
{
  m_leftWindowPos = pos;
}


void EditorSettings::setRightWindowPos(WindowPosition const &pos)
{
  m_rightWindowPos = pos;
}


// ------------------------------- misc --------------------------------
void EditorSettings::setGrepsrcSearchesSubrepos(bool b)
{
  m_grepsrcSearchesSubrepos = b;
}


// EOF
