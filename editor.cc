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
#include <qrangecontrol.h>   // QRangeControl

#include <stdio.h>           // printf, for debugging


// ---------------------- Editor --------------------------------
Editor::Editor(Buffer *buf,
               QWidget *parent=NULL, const char *name=NULL)
  : QWidget(parent, name, WRepaintNoErase | WNorthWestGravity),
    horizScroll(NULL),
    vertScroll(NULL),
    buffer(buf),
    cursor(buf),
    // visible line/col inited by resetView()
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    ctrlShiftDistance(10)
    // font metrics inited by setFont()
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);

  // use the color scheme for text widgets; typically this means
  // a white background, instead of a gray background
  setBackgroundMode(PaletteBase);
  
  resetView();
}


void Editor::setFont(QFont &f)
{
  QWidget::setFont(f);

  // info about the current font
  QFontMetrics fm(fontMetrics());

  ascent = fm.ascent();
  descent = fm.descent() + 1;
  fontHeight = ascent + descent;
  fontWidth = fm.maxWidth();

  // natural # of blank pixels between lines
  //int leading = fm.leading();
  //printf("ascent=%d descent=%d fontHeight=%d fontWidth=%d leading=%d\n",
  //       ascent, descent, fontHeight, fontWidth, leading);
}


void Editor::update()
{                   
  updateView();
  QWidget::update();
}

void Editor::updateView()
{
  // calculate viewport stats
  // why -1?  suppose width==height==0, then the "first" visible isn't
  // visible at all, so we'd want the one before (not that that's visible
  // either, but it suggests what we want in nondegenerate cases too)
  lastVisibleLine = firstVisibleLine
    + (height() - topMargin) / (fontHeight + interLineSpace) - 1;
  lastVisibleCol = firstVisibleCol
    + (width() - leftMargin) / fontWidth - 1;

  // set the scrollbars
  if (horizScroll) {
    horizScroll->setRange(0, max(buffer->totColumns(), firstVisibleCol));
    horizScroll->setSteps(1, visCols());
    horizScroll->setValue(firstVisibleCol);
  }

  if (vertScroll) {
    vertScroll->setRange(0, max(buffer->totLines(), firstVisibleLine));
    vertScroll->setSteps(1, visLines());
    vertScroll->setValue(firstVisibleLine);
  }
}


void Editor::resizeEvent(QResizeEvent *r)
{
  QWidget::resizeEvent(r);
  updateView();
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
  // make a painter
  QPainter paint(this);

  // when drawing text, erase background automatically
  paint.setBackgroundMode(OpaqueMode);

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
      paint.drawLine(x-1,y, x-1, y+fontHeight-1);
    }
    
    y += fontHeight + interLineSpace;
  }

  // fill the remainder
  paint.eraseRect(0, y,
                  width(), height()-y);
}


// increment, but don't allow result to go below 0
static void inc(int &val, int amt)
{
  val += amt;
  if (val < 0) {
    val = 0;
  }
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

      case Key_PageUp:
        cursor.set(0,0);
        scrollToCursor();
        update();
        break;

      case Key_PageDown:
        cursor.set(max(buffer->totLines()-1,0), 0);
        scrollToCursor();
        update();
        break;

      case Key_W:
      case Key_Up:
        moveView(-1, 0);
        break;

      case Key_Z:
      case Key_Down:
        moveView(+1, 0);
        break;

      case Key_Left:
        moveView(0, -1);
        break;

      case Key_Right:
        moveView(0, +1);
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Ctrl+Shift+<key>
  else if (state == (ControlButton|ShiftButton)) {
    xassert(ctrlShiftDistance > 0);

    switch (k->key()) {
      case Key_Up:
        moveView(-ctrlShiftDistance, 0);
        break;

      case Key_Down:
        moveView(+ctrlShiftDistance, 0);
        break;

      case Key_Left:
        moveView(0, -ctrlShiftDistance);
        break;

      case Key_Right:
        moveView(0, +ctrlShiftDistance);
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

      case Key_PageUp:
        moveView(-visLines(), 0);
        break;

      case Key_PageDown:
        moveView(+visLines(), 0);
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


void Editor::moveView(int deltaLine, int deltaCol)
{
  // first make sure the view contains the cursor
  scrollToCursor();
  
  // move viewport, but remember original so we can tell
  // when there's truncation
  int origVL = firstVisibleLine;
  int origVC = firstVisibleCol;
  inc(firstVisibleLine, deltaLine);
  inc(firstVisibleCol, deltaCol);

  // now move cursor by the amount that the viewport moved
  cursor.move(firstVisibleLine-origVL, 
              firstVisibleCol-origVC);

  // redraw display
  update();
}


void Editor::scrollToLine(int line)
{
  xassert(line >= 0);
  firstVisibleLine = line;
  update();
}

void Editor::scrollToCol(int col)
{
  xassert(col >= 0);
  firstVisibleCol = col;
  update();
}


void Editor::mousePressEvent(QMouseEvent *m)
{
  // get rid of popups?
  QWidget::mousePressEvent(m);

  int x = m->x();
  int y = m->y();
                      
  // subtract off the margin, but don't let either coord go negative
  inc(x, -leftMargin);                                   
  inc(y, -topMargin);

  int newLine = y/(fontHeight+interLineSpace) + firstVisibleLine;
  int newCol = x/fontWidth + firstVisibleCol;

  //printf("click: (%d,%d)     goto line %d, col %d\n",
  //       x, y, newLine, newCol);

  cursor.set(newLine, newCol);

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
  
  update();
}
