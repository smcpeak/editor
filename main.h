// main.h
// editor main GUI window

#ifndef MAIN_H
#define MAIN_H

#include <qwidget.h>       // QWidget

#include "bufferstate.h"   // BufferState

class QMenuBar;
class QScrollBar;
class QLabel;
class Editor;              // editor.h


class EditorWindow : public QWidget {
  Q_OBJECT

public:      // data
  // GUI elements
  QMenuBar *menuBar;
  Editor *editor;
  QScrollBar *vertScroll, *horizScroll;
  QLabel *position, *filename;

  // right now, one buffer
  BufferState theBuffer;

private:     // funcs
  void writeTheFile();

public:      // funcs
  EditorWindow(QWidget *parent=0, char const *name=0);

public slots:
  void fileNew();
  void fileOpen();
  void fileSave();
  void fileSaveAs();
  void helpAbout();
};


#endif // MAIN_H
