// main.cc
// editor main GUI window

#include "editor.h"          // Editor
#include "buffer.h"          // Buffer

#include <qapplication.h>    // QApplication


// right now, one buffer
Buffer theBuffer;


int main(int argc, char **argv)
{
  QApplication a(argc, argv);

  // insert some dummy text into the buffer
  {
    Position start(&theBuffer);
    theBuffer.insertText(start, "hi there", 8);
  }

  Editor editor(&theBuffer, NULL /*parent*/, "An Editor");
  editor.resize(300,300);

  a.setMainWidget(&editor);
  editor.show();
  return a.exec();
}
