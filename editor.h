// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget

#include "position.h"       // Position

class Buffer;               // buffer.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor : public QWidget {
  Q_OBJECT

public:      // data
  // buffer whose text we're editing
  Buffer *buffer;           // (serf)

  // cursor position
  Position cursor;

protected:   // funcs
  // respond to a paint request by repainting the control
  virtual void paintEvent(QPaintEvent *);
  
  // handle keyboard input
  virtual void keyPressEvent(QKeyEvent *k);

public:      // funcs
  Editor(Buffer *buf,
         QWidget *parent=NULL, const char *name=NULL);
};

#endif // EDITOR_H
