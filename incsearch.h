// incsearch.h
// input proxy that implements incremental search

#ifndef INCSEARCH_H
#define INCSEARCH_H

#include "inputproxy.h"     // AttachInputProxy
#include "str.h"            // string
#include <qstring.h>        // QString

class QLabel;               // qlabel.h


class IncSearch : public AttachInputProxy {
private:
  // label for reporing status
  QLabel *status;              // (nullable serf)
  QString prevStatusText;      // text previously in 'status'

  // cursor location where search began
  int beginLine, beginCol;

  // view position where search began
  int beginFVLine, beginFVCol;

  // ----- transient state: what I'd have before implementing Backspace -----
  // text we're searching for
  string text;

  // cursor location; beginning of current match if there is
  // a match, or beginning of closest-match prefix
  int curLine, curCol;

public:
  IncSearch(QLabel *status);
  ~IncSearch();

  // AttachInputProxy funcs
  void attach(Editor *ed);
  virtual void detach();
  virtual bool keyPressEvent(QKeyEvent *k);
};


#endif // INCSEARCH_H
