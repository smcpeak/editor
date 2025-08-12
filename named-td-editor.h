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
public:    // data
  // Process-wide record of the open file.  Not an owner pointer.
  // Must not be null.
  RCSerf<NamedTextDocument> m_namedDoc;

public:
  ~NamedTextDocumentEditor();

  // Build an editor for the given document.
  NamedTextDocumentEditor(NamedTextDocument *d);
};


#endif // EDITOR_NAMED_TD_EDITOR_H
