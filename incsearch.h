// incsearch.h
// input proxy that implements incremental search

#ifndef INCSEARCH_H
#define INCSEARCH_H

#include "inputproxy.h"                // AttachInputProxy
#include "text-document-editor.h"      // TextDocumentEditor

#include "str.h"                       // string

#include <qstring.h>                   // QString

class StatusDisplay;                   // status.h


class IncSearch : public AttachInputProxy {
private:     // data
  // label for reporing status
  StatusDisplay *status;       // (nullable serf)
  QString prevStatusText;      // text previously in 'status'

  // cursor location where search began
  int beginLine, beginCol;

  // view position where search began
  int beginFVLine, beginFVCol;

  // current search options
  TextDocumentEditor::FindStringFlags curFlags;

  // text we're searching for
  string text;

  // cursor location; beginning of current match if there is
  // a match, or beginning of closest-match prefix
  int curLine, curCol;

  // whether the last search found a match
  bool match;

  // mode
  enum Mode {
    M_SEARCH,           // searching for matching text
    M_GET_REPLACEMENT,  // get the replacement text
    M_REPLACE,          // applying replacement text
  } mode;

  // text removed during M_GET_REPLACEMENT
  string removedText;

  // replacement text
  string replaceText;

private:     // funcs
  // return the editor to the cursor,view that it had
  // when the search began
  void resetToSearchStart();

  // search from curLine/curCol to find 'text'; return true
  // if we find a match
  bool findString(TextDocumentEditor::FindStringFlags flags);
  bool findString() { return findString(curFlags); }

  // find next/prev occurrence, return true if found
  bool nextMatch();
  bool prevMatch();

  // string for status line
  QString statusText() const;

  // put statusText() in the label, if it exists
  void updateStatus();

  // attempt to find a match by wrapping to end/beginning of file;
  // don't update internal stats, just set line/col and return true
  // if a match is found
  bool tryWrapSearch(int &line, int &col) const;

  // key handlers for the three modes
  bool searchKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state);
  bool getReplacementKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state);
  bool replaceKeyMap(QKeyEvent *k, Qt::KeyboardModifiers state);

  // pseudo key handlers
  bool searchPseudoKey(InputPseudoKey pkey);
  bool getReplacementPseudoKey(InputPseudoKey pkey);
  bool replacePseudoKey(InputPseudoKey pkey);

  // switch modes, and update status line
  void setMode(Mode m);

  // delete selection, insert 'text'
  void putBackMatchText();

  // replace current match with 'replaceText', go to next
  // match; return false and call detach() if no such match
  bool replace();

public:      // funcs
  IncSearch(StatusDisplay *status);
  ~IncSearch();

  // AttachInputProxy funcs
  void attach(EditorWidget *ed);
  virtual void detach();
  virtual bool keyPressEvent(QKeyEvent *k);
  virtual bool pseudoKeyPress(InputPseudoKey pkey);
};


#endif // INCSEARCH_H
