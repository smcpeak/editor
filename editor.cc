// editor.cc
// code for editor.h

#include "editor.h"          // this module
#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine
#include "xassert.h"         // xassert
#include "array.h"           // Array

#include <qapplication.h>    // QApplication
#include <qpainter.h>        // QPainter
#include <qfontmetrics.h>    // QFontMetrics
#include <qrangecontrol.h>   // QRangeControl

#include <stdio.h>           // printf, for debugging


// ---------------------- Editor --------------------------------
Editor::Editor(BufferState *buf,
               QWidget *parent=NULL, const char *name=NULL)
  : QWidget(parent, name, WRepaintNoErase | WNorthWestGravity),
    buffer(buf),
    // cursor and visible line/col inited by resetView()
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    ctrlShiftDistance(10)
    // font metrics inited by setFont()
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);

  setCursor(ibeamCursor);

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
  emit viewChanged();
  QWidget::update();
}

void Editor::updateView()
{
  int h = height();
  int w = width();

  // calculate viewport stats
  // why -1?  suppose width==height==0, then the "first" visible isn't
  // visible at all, so we'd want the one before (not that that's visible
  // either, but it suggests what we want in nondegenerate cases too)
  lastVisibleLine = firstVisibleLine
    + (h - topMargin) / (fontHeight + interLineSpace) - 1;
  lastVisibleCol = firstVisibleCol
    + (w - leftMargin) / fontWidth - 1;
}


void Editor::resizeEvent(QResizeEvent *r)
{
  QWidget::resizeEvent(r);
  updateView();
  emit viewChanged();
}


void Editor::resetView()
{
  cursorLine = 0;
  cursorCol = 0;
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
  int lastPrintLine = max(buffer->numLines()-1, cursorLine);

  // buffer for text to print
  int visibleCols = lastVisibleCol - firstVisibleCol + 2;
  Array<char> text(visibleCols);

  for (int line = firstVisibleLine;
       line <= lastPrintLine && y < height();
       line++) {
    // figure out what to draw
    int len = 0;

    if (line < buffer->numLines() &&
        firstVisibleCol < buffer->lineLength(line)) {
      len = min(buffer->lineLength(line) - firstVisibleCol, visibleCols);
      buffer->getLine(line, firstVisibleCol, text, len);
    }

    // draw line's text
    if (len > 0) {
      paint.drawText(leftMargin, y+ascent-1,      // baseline start coordinate
                     QString(text), len);         // text, length
    }

    // right edge of what has not been painted
    int x = leftMargin + fontWidth * len;

    // erase to right edge of the window
    paint.eraseRect(x, y,
                    width()-x, fontHeight);

    // draw the cursor as a line
    if (line == cursorLine) {
      x = leftMargin + fontWidth * (cursorCol - firstVisibleCol);
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
        cursorLine = 0;
        cursorCol = 0;
        scrollToCursor();
        update();
        break;

      case Key_PageDown:
        cursorLine = max(buffer->numLines(),0);
        cursorCol = 0;
        scrollToCursor();
        update();
        break;

      case Key_W:
        inc(firstVisibleLine, -1);
        updateView();
        if (cursorLine > lastVisibleLine) {
          cursorLine = lastVisibleLine;
        }
        update();
        break;

      case Key_Z:
        inc(firstVisibleLine, +1);
        updateView();
        if (cursorLine < firstVisibleLine) {
          cursorLine = firstVisibleLine;
        }
        update();
        break;

      case Key_Up:
        moveView(-1, 0);
        break;

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
        inc(cursorCol, -1);
        scrollToCursor();
        break;

      case Key_Right:
        inc(cursorCol, +1);
        scrollToCursor();
        break;

      case Key_Home:
        cursorCol = 0;
        scrollToCursor();
        break;

      case Key_End:
        cursorCol = buffer->lineLength(cursorLine);
        scrollToCursor();
        break;

      case Key_Up:
        inc(cursorLine, -1);
        scrollToCursor();
        break;

      case Key_Down:
        // allows cursor past EOF..
        inc(cursorLine, +1);
        scrollToCursor();
        break;

      case Key_PageUp:
        moveView(-visLines(), 0);
        break;

      case Key_PageDown:
        moveView(+visLines(), 0);
        break;

      case Key_BackSpace: {
        fillToCursor();
        buffer->changed = true;

        if (cursorCol == 0) {
          if (cursorLine == 0) {
            // do nothing
          }
          else {
            // move to end of previous line
            cursorLine--;
            cursorCol = buffer->lineLength(cursorLine);

            // splice them together
            spliceNextLine();
          }
        }
        else {
          // remove the character to the left of the cursor
          cursorCol--;
          buffer->deleteText(cursorLine, cursorCol, 1);
        }

        scrollToCursor();
        break;
      }

      case Key_Delete: {
        fillToCursor();
        buffer->changed = true;

        if (cursorCol == buffer->lineLength(cursorLine)) {
          // splice next line onto this one
          spliceNextLine();
        }
        else /* cursor < lineLength */ {
          // delete character to right of cursor
          buffer->deleteText(cursorLine, cursorCol, 1);
        }

        scrollToCursor();
        break;
      }

      case Key_Return: {
        fillToCursor();
        buffer->changed = true;

        if (cursorCol == buffer->lineLength(cursorLine)) {
          // add a blank line after this one
          cursorLine++;
          cursorCol = 0;
          buffer->insertLine(cursorLine);
        }
        else if (cursorCol == 0) {
          // insert blank line and go to the next
          buffer->insertLine(cursorLine);
          cursorLine++;
        }
        else {
          // steal text after the cursor
          int len = buffer->lineLength(cursorLine) - cursorCol;
          Array<char> temp(len);
          buffer->getLine(cursorLine, cursorCol, temp, len);
          buffer->deleteText(cursorLine, cursorCol, len);

          // insert a line and move down
          cursorLine++;
          cursorCol = 0;
          buffer->insertLine(cursorLine);

          // insert the stolen text
          buffer->insertText(cursorLine, 0, temp, len);
        }

        scrollToCursor();
        break;
      }

      default: {
        QString text = k->text();
        if (text.length()) {
          fillToCursor();
          buffer->changed = true;

          // insert this character at the cursor
          buffer->insertText(cursorLine, cursorCol, text, text.length());
          cursorCol += text.length();
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


void Editor::fillToCursor()
{                                         
  // fill with blank lines
  while (cursorLine >= buffer->numLines()) {
    buffer->insertLine(buffer->numLines());
  }

  // fill with spaces
  while (cursorCol > buffer->lineLength(cursorLine)) {
    buffer->insertText(cursorLine, buffer->lineLength(cursorLine),
                       " ", 1);
  }
}


void Editor::spliceNextLine()
{
  // cursor must be at the end of a line
  xassert(cursorCol == buffer->lineLength(cursorLine));

  // splice this line with the next
  if (cursorLine+1 < buffer->numLines()) {
    // append the contents of the next line
    int len = buffer->lineLength(cursorLine+1);
    Array<char> temp(len);
    buffer->getLine(cursorLine+1, 0 /*col*/, temp, len);
    buffer->insertText(cursorLine, cursorCol, temp, len);

    // now remove the next line
    buffer->deleteText(cursorLine+1, 0 /*col*/, len);
    buffer->deleteLine(cursorLine+1);
  }
}


void Editor::scrollToCursor()
{
  if (cursorCol < firstVisibleCol) {
    firstVisibleCol = cursorCol;
  }
  else if (cursorCol > lastVisibleCol) {
    firstVisibleCol += (cursorCol - lastVisibleCol);
  }

  if (cursorLine < firstVisibleLine) {
    firstVisibleLine = cursorLine;
  }
  else if (cursorLine > lastVisibleLine) {
    firstVisibleLine += (cursorLine - lastVisibleLine);
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
  inc(cursorLine, firstVisibleLine-origVL);
  inc(cursorCol, firstVisibleCol-origVC);

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

  cursorLine = newLine;
  cursorCol = newCol;

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
  
  update();
}
