// bufferstate.h
// a Buffer, plus some state suitable for an editor

// in an editor, the BufferState would contain all the info that is
// remembered for *undisplayed* buffers

#ifndef BUFFERSTATE_H
#define BUFFERSTATE_H

#include "buffer.h"     // Buffer
#include "str.h"        // string
#include "hilite.h"     // Highlighter

                  
// editor widget editing state, saved so it can be restored
// when the user switches back to this buffer
class EditingState {
public:      // data
  // cursor position (0-based)
  //int cursorLine;
  //int cursorCol;
  // UPDATE: The cursor has been moved into Buffer itself (via
  // HistoryBuffer and CursorBuffer), and so is no longer present
  // in this class.

  // selection state: a location, and a flag to enable it
  int selectLine;
  int selectCol;
  bool selectEnabled;

  #if 0     // moved back into Editor
  // the following fields are valid only after normalizeSelect(),
  // and before any subsequent modification to cursor or select
  int selLowLine, selLowCol;     // whichever of cursor/select comes first
  int selHighLine, selHighCol;   // whichever comes second
  #endif // 0

  // scrolling offset; must change via setView()
  int const firstVisibleLine, firstVisibleCol;

  // information about viewable area; these are set by the
  // Editor::updateView() routine and should be treated as read-only by
  // other code; by "visible", I mean the entire line or column is visible
  int lastVisibleLine, lastVisibleCol;

  // when nonempty, any buffer text matching this string will
  // be highlighted in the 'hit' style; match is carried out
  // under influence of 'hitTextFlags'
  string hitText;
  Buffer::FindStringFlags hitTextFlags;

protected:   // funcs
  // set firstVisibleLine/Col; for internal use by
  // EditingState::copy, and Editor::setView()
  void setFirstVisibleLC(int newFirstLine, int newFirstCol);

public:      // funcs
  EditingState();
  ~EditingState();

  // copy editing state from 'src'
  void copy(EditingState const &src);
};
  

class BufferState : public Buffer {
public:      // data
  // name of file being edited
  string filename;

  // title of the buffer; this will usually be similar
  // to the filename, but perhaps only the last part of
  // the fully-qualified path name, etc.
  string title;

  // id of the menu item that the user can choose to
  // select this buffer
  int windowMenuId;
  
  // key code the user can press to jump to this buffer,
  // or 0 for no hotkey
  int hotkey;

  // true when there are unsaved changes
  //bool changed;                       
  // replaced with unsavedChanges() method

  // current highlighter; clients can come in and replace the
  // highlighter, but it must always be the case that the
  // highlighter is attached to 'this' buffer (because it's allowed
  // to maintain internal incremental state about the buffer
  // contents)
  Highlighter *highlighter;      // (nullable owner)

  // saved editing state
  EditingState savedState;
  
public:      // funcs
  BufferState();
  ~BufferState();
};

#endif // BUFFERSTATE_H
