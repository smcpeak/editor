// editor.cc
// code for editor.h

#include "editor.h"          // this module
#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine
#include "xassert.h"         // xassert

#include <qapplication.h>    // QApplication
#include <qpainter.h>        // QPainter
#include <qfontmetrics.h>    // QFontMetrics

#include <stdio.h>           // printf, for debugging


// ---------------------- Editor --------------------------------
Editor::Editor(Buffer *buf,
               QWidget *parent=NULL, const char *name=NULL)        
    // turned off gravity because with it I don't get repaint events
    // while resizing inward, which means I don't recompute the
    // visible line/col ...
  : QWidget(parent, name, WRepaintNoErase /*| WNorthWestGravity*/),
    buffer(buf),
    cursor(buf),
    // visible line/col inited by resetView
    topMargin(1),
    leftMargin(1),
    interLineSpace(0)
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);

  // use the color scheme for text widgets; typically this means
  // a white background, instead of a gray background
  setBackgroundMode(PaletteBase);
  
  resetView();
}


void Editor::resetView()
{
  cursor.set(0,0);
  firstVisibleLine = 0;
  firstVisibleCol = 0;
}


// In general, to avoid flickering, I try to paint every pixel
// exactly once (this idea comes straight from the Qt tutorial).
// So far, the only place I violate that is the cursor, the pixels
// of which are drawn twice when it is visible.
void Editor::paintEvent(QPaintEvent *ev)
{
  // make a painter, and get info about the current font
  QPainter paint(this);
  QFontMetrics fm(paint.fontMetrics());

  // when drawing text, erase background automatically
  paint.setBackgroundMode(OpaqueMode);

  // number of pixels in a character cell that are above the
  // base line, including the base line itself
  int ascent = fm.ascent();

  // number of pixels below the base line, not including the
  // base line itself
  int descent = fm.descent() + 1;

  // total # of pixels in each cell
  int fontHeight = ascent + descent;
  int fontWidth = fm.maxWidth();

  // natural # of blank pixels between lines
  //int leading = fm.leading();
  //printf("ascent=%d descent=%d fontHeight=%d fontWidth=%d leading=%d\n",
  //       ascent, descent, fontHeight, fontWidth, leading);

  // top edge of what has not been painted
  int y = 0;

  if (topMargin) {
    paint.eraseRect(0, y, width(), topMargin);
    y += topMargin;
  }

  if (leftMargin) {
    paint.eraseRect(0, 0, leftMargin, height());
  }

  // I think it might be useful to support negative values for these
  // variables, but the code below is not prepared to deal with such
  // values at this time
  xassert(firstVisibleLine >= 0);
  xassert(firstVisibleCol >= 0);

  // another santiy check
  xassert(fontHeight + interLineSpace > 0);

  // last line where there's something to print
  int lastPrintLine = max(buffer->totLines()-1, cursor.line());

  for (int line = firstVisibleLine;
       line <= lastPrintLine && y < height();
       line++) {
    // figure out what to draw
    char const *text = NULL;
    int len = 0;
    if (line < buffer->totLines()) {
      TextLine const *tl = buffer->getLineC(line);
      text = tl->getText();
      len = tl->getLength();

      if (firstVisibleCol < len) {
        text += firstVisibleCol;
        len -= firstVisibleCol;
      }
      else {
        len = 0;
      }
    }

    // draw line's text
    if (len > 0) {
      paint.drawText(leftMargin, y+ascent-1,      // baseline start coordinate
                     text, len);                  // text, length
    }

    // right edge of what has not been painted
    int x = leftMargin + fontWidth * len;

    // erase to right edge of the window
    paint.eraseRect(x, y,
                    width()-x, fontHeight);

    // draw the cursor as a line
    if (line == cursor.line()) {
      x = leftMargin + fontWidth * (cursor.col() - firstVisibleCol);
      paint.drawLine(x,y, x, y+fontHeight-1);
    }
    
    y += fontHeight + interLineSpace;
  }

  // fill the remainder
  paint.eraseRect(0, y,
                  width(), height()-y);
                  
  // calculate viewport stats
  // why -1?  suppose width==height==0, then the "first" visible isn't
  // visible at all, so we'd want the one before (not that that's visible
  // either, but it suggests what we want in nondegenerate cases too)
  lastVisibleLine = firstVisibleLine
    + (height() - topMargin) / (fontHeight + interLineSpace) - 1;
  lastVisibleCol = firstVisibleCol
    + (width() - leftMargin) / fontWidth - 1;
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
      case Key_Left:       
        cursor.move(0,-1);
        scrollToCursor();
        break;

      case Key_Right:
        cursor.move(0,+1);
        scrollToCursor();
        break;

      case Key_Home:
        cursor.setCol(0);
        scrollToCursor();
        break;

      case Key_End:
        cursor.setCol(buffer->getLineC(cursor.line())->getLength());
        scrollToCursor();
        break;

      case Key_Up:
        cursor.move(-1,0);
        scrollToCursor();
        break;

      case Key_Down:
        // allows cursor past EOF..
        cursor.move(+1,0);
        scrollToCursor();
        break;

      case Key_BackSpace: {
        Position oneLeft(cursor);
        oneLeft.moveLeftWrap();
        buffer->deleteText(oneLeft, cursor);
        scrollToCursor();
        break;
      }

      case Key_Delete: {
        Position oneRight(cursor);
        oneRight.moveRightWrap();
        buffer->deleteText(cursor, oneRight);
        scrollToCursor();
        break;
      }

      case Key_Return: {
        buffer->insertText(cursor, "\n", 1);
        scrollToCursor();
        break;
      }

      default: {
        QString text = k->text();
        if (text.length()) {
          // insert this character at the cursor
          buffer->insertText(cursor, text, text.length());
          scrollToCursor();
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


void Editor::scrollToCursor()
{
  if (cursor.col() < firstVisibleCol) {
    firstVisibleCol = cursor.col();
  }
  else if (cursor.col() > lastVisibleCol) {
    firstVisibleCol += (cursor.col() - lastVisibleCol);
  }

  if (cursor.line() < firstVisibleLine) {
    firstVisibleLine = cursor.line();
  }
  else if (cursor.line() > lastVisibleLine) {
    firstVisibleLine += (cursor.line() - lastVisibleLine);
  }
}
