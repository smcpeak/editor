// editor.cc
// code for editor.h

#include "editor.h"          // this module

#include <qapplication.h>    // QApplication
#include <qpainter.h>        // QPainter
#include <qfontmetrics.h>    // QFontMetrics

#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine


// ---------------------- Editor --------------------------------
Editor::Editor(Buffer *buf,
               QWidget *parent=NULL, const char *name=NULL)
  : QWidget(parent, name),
    buffer(buf),
    cursor(buf)
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);
}

void Editor::paintEvent(QPaintEvent *ev)
{
  // make a painter, and get info about the current font
  QPainter paint(this);
  QFontMetrics fm(paint.fontMetrics());

  // when drawing text, erase background automatically
  paint.setBackgroundMode(OpaqueMode);

  // use my font!
  //XSetFont(win.display, gc, myFontId);

  //XFontStruct *fs = myFontStruct;
  int ascent = fm.ascent();
  int descent = fm.descent() + 1;    // this is X's notion of descent
  int fontHeight = ascent + descent;
  int fontWidth = fm.maxWidth();

  int lastBotDescent = 0;

  int leftMargin = 5;
  int topMargin = 5;

  for (int line=0; line < buffer->totLines(); line++) {
    int baseLine = topMargin + (line+1)*fontHeight;
    //int topAscent = baseLine - fs->ascent;
    int botDescent = baseLine + descent;
    lastBotDescent = botDescent;

    // erase left margin
    paint.drawText(0, baseLine, QString(" "), 1);

    // draw line's text
    TextLine const *tl = buffer->getLineC(line);
    QString text(tl->getText());
    paint.drawText(leftMargin, baseLine,        // lower,base coordinate
                   text, tl->getLength());      // text, length

    // calc width of whole string
    int width = fm.width(text, tl->getLength());

    // write spaces to right edge of the window
    paint.eraseRect(leftMargin+width, baseLine-ascent,
                    this->width(), fontHeight);
  }

  // fill the remainder
  paint.eraseRect(0, lastBotDescent,
                  width(), height());

  // draw the cursor as a line
  {
    int line = cursor.line();
    int baseLine = topMargin + (line+1)*fontHeight;
    int topAscent = baseLine - ascent;
    int botDescent = baseLine + descent - 1;
    int width = fontWidth * cursor.col();
    paint.drawLine(leftMargin+width-1, botDescent,     // bottom of descent
                   leftMargin+width-1, topAscent);     // top of ascent
  }

  // cue to quit app
  char const *msg = "Ctrl-C to quit";
  paint.drawText(
    5,295,                     // left edge, baseline coordinate
    msg                        // text
  );
}


void Editor::keyPressEvent(QKeyEvent *k)
{     
  int state = k->state() & KeyButtonMask;
                 
  // control-<key>
  if (state == ControlButton) {
    switch (k->key()) {
      case Key_U:
        buffer->dumpRepresentation();
        break;
        
      case Key_C:
        QApplication::exit();     // this *does* return
        break;
        
      default:
        k->ignore();
        break;
    }         
  }
                        
  // <key>
  else if (state == NoButton || state == ShiftButton) {
    switch (k->key()) {
      case Key_Left:       cursor.move(0,-1);    break;
      case Key_Right:      cursor.move(0,+1);    break;
      case Key_Home:       cursor.setCol(0);     break;
      case Key_End:
        cursor.setCol(
          buffer->getLineC(cursor.line())->getLength());
        break;
      case Key_Up:         cursor.move(-1,0);    break;
      case Key_Down:       cursor.move(+1,0);    break;  // allows cursor past EOF..

      case Key_BackSpace: {
        Position oneLeft(cursor);
        oneLeft.moveLeftWrap();
        buffer->deleteText(oneLeft, cursor);
        break;
      }

      case Key_Delete: {
        Position oneRight(cursor);
        oneRight.moveRightWrap();
        buffer->deleteText(cursor, oneRight);
        break;
      }

      case Key_Return: {
        buffer->insertText(cursor, "\n", 1);
        break;
      }

      default: {
        QString text = k->text();
        if (text.length()) {
          // insert this character at the cursor
          buffer->insertText(cursor, text, text.length());
        }
        else {   
          k->ignore();
          return;     
        }
      }
    }

    // redraw
    update();
  }

  // other combinations
  else {
    k->ignore();
  }
}
