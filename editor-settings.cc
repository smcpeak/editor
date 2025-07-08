// editor-settings.cc
// Code for editor-settings.h.

#include "editor-settings.h"           // this module

#include "editor-command.ast.gen.h"    // EditorCommand

// Ensure the required templates are all declared before use.
#include "smbase/gdvalue-map-fwd.h"
#include "smbase/gdvalue-unique-ptr-fwd.h"
#include "smbase/gdvalue-vector-fwd.h"

#include "smbase/container-util.h"     // smbase::contains
#include "smbase/gdvalue-map.h"        // GDValue <-> std::map
#include "smbase/gdvalue-parse-ops.h"  // mapGetSym_parse{,Opt}
#include "smbase/gdvalue-unique-ptr.h" // GDValue <-> std::unique_ptr
#include "smbase/gdvalue-vector.h"     // GDValue <-> std::vector
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // keySet
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <utility>                     // std::swap


using namespace gdv;
using namespace smbase;


INIT_TRACE("editor-settings");


// Version number for the settings file format.
static int const CUR_VERSION = 1;


EditorSettings::~EditorSettings()
{}


EditorSettings::EditorSettings()
  : m_macros()
{}


EditorSettings::EditorSettings(GDValue const &m)
  : m_macros(gdvTo<MacroDefinitionMap>(mapGetSym_parse(m, "macros"))),

    // This was added after creating the initial version.  I'm going to
    // try having the parser just accept a missing field.
    m_mostRecentlyRunMacro(gdvOptTo<std::string>(
      mapGetSym_parseOpt(m, "mostRecentlyRunMacro")))
{
  checkTaggedOrderedMapTag(m, "EditorSettings");

  int version = gdvTo<int>(mapGetSym_parse(m, "version"));
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

  m.mapSetSym("version", CUR_VERSION);
  m.mapSetSym("macros", toGDValue(m_macros));
  m.mapSetSym("mostRecentlyRunMacro", toGDValue(m_mostRecentlyRunMacro));

  return m;
}


void EditorSettings::swap(EditorSettings &obj)
{
  if (this != &obj) {
    using std::swap;

    swap(m_macros, obj.m_macros);
    swap(m_mostRecentlyRunMacro, obj.m_mostRecentlyRunMacro);
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
  return keySet(m_macros);
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
           " not among macro keys: " << toGDValue(keySet(m_macros)));

    m_mostRecentlyRunMacro.clear();
    return {};
  }
}


// EOF
