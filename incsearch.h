// incsearch.h
// input proxy that implements incremental search

#ifndef INCSEARCH_H
#define INCSEARCH_H

#include "inputproxy.h"     // AttachInputProxy
#include "str.h"            // string
#include "buffer.h"         // Buffer
#include <qstring.h>        // QString

class QLabel;               // qlabel.h


class IncSearch : public AttachInputProxy {
private:     // data
  // label for reporing status
  QLabel *status;              // (nullable serf)
  QString prevStatusText;      // text previously in 'status'

  // cursor location where search began
  int beginLine, beginCol;

  // view position where search began
  int beginFVLine, beginFVCol;

  // current search options
  Buffer::FindStringFlags curFlags;

  // ----- transient state: what I'd have before implementing Backspace -----
  // text we're searching for
  string text;

  // cursor location; beginning of current match if there is
  // a match, or beginning of closest-match prefix
  int curLine, curCol;

  // whether the last search found a match
  bool match;

private:     // funcs
  // search from curLine/curCol to find 'text'; return true
  // if we find a match
  bool findString(Buffer::FindStringFlags flags);
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

public:      // funcs
  IncSearch(QLabel *status);
  ~IncSearch();

  // AttachInputProxy funcs
  void attach(Editor *ed);
  virtual void detach();
  virtual bool keyPressEvent(QKeyEvent *k);
};


#endif // INCSEARCH_H
