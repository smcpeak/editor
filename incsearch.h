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

  // buffer text removed during M_GET_REPLACEMENT
  string removedText;
    
  // replacement text
  string replaceText;
  
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

  // key handlers for the three modes
  bool searchKeyMap(QKeyEvent *k, int state);
  bool getReplacementKeyMap(QKeyEvent *k, int state);
  bool replaceKeyMap(QKeyEvent *k, int state);
  
  // switch modes, and update status line
  void setMode(Mode m);
                 
  // delete selection, insert 'text'
  void putBackMatchText();
  
  // replace current match with 'replaceText', go to next
  // match; return false and call detach() if no such match 
  bool replace();
    
public:      // funcs
  IncSearch(QLabel *status);
  ~IncSearch();

  // AttachInputProxy funcs
  void attach(Editor *ed);
  virtual void detach();
  virtual bool keyPressEvent(QKeyEvent *k);
};


#endif // INCSEARCH_H
