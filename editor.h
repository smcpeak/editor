// editor.h
// widget for editing text

#ifndef EDITOR_H
#define EDITOR_H

#include <qwidget.h>        // QWidget
         
class Buffer;               // buffer.h


// control to edit the contents of a buffer; it's possible (and
// expected) to change which buffer a given Editor edits after
// creating the Editor object
class Editor : public QWidget {
  Q_OBJECT

public:      // data
  // buffer whose text we're editing
  Buffer *buffer;           // (serf)

protected:   // funcs
  // respond to a paint request by repainting the control
  virtual void paintEvent(QPaintEvent *);

public:      // funcs
  Editor(Buffer *buf,
         QWidget *parent=NULL, const char *name=NULL)
    : QWidget(parent, name),
      buffer(buf)
  {}
};

#endif // EDITOR_H
