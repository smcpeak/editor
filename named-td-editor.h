// named-td-editor.h
// `NamedTextDocumentEditor`: A widget-specific `TextDocumentEditor`
// for a specific `NamedTextDocument`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_NAMED_TD_EDITOR_H
#define EDITOR_NAMED_TD_EDITOR_H

#include "named-td-editor-fwd.h"       // NamedTextDocumentEditor

#include "smbase/refct-serf.h"         // RCSerf

#include "td-editor.h"                 // TextDocumentEditor
#include "named-td-fwd.h"              // NamedTextDocument


// For a particular `EditorWidget`, and for a given `NamedTextDocument`,
// this is the editing state for that document.  This state is *not*
// shared with other widgets in the editor application, although it
// contains a pointer to a `NamedTextDocument`, which *is* shared.
class NamedTextDocumentEditor : public TextDocumentEditor {
public:    // class data
  // A string containing one line for each of the substitutions that
  // `applyCommandSubstitution` can perform.  This gets included in the
  // help text of `ApplyCommandDialog`.
  static char const * const s_substitutionsHelpString;

public:    // instance data
  // Process-wide record of the open file.  Not an owner pointer.
  // Must not be null.
  RCSerf<NamedTextDocument> m_namedDoc;

public:
  ~NamedTextDocumentEditor();

  // Build an editor for the given document.
  NamedTextDocumentEditor(NamedTextDocument *d);

  // Replace occurrences of variables like "$f" with their meanings,
  // which derive from things like the file name.  See the definition of
  // `s_substitutionsHelpString`.
  string applyCommandSubstitutions(string const &orig) const;
};


#endif // EDITOR_NAMED_TD_EDITOR_H
