// editor.cc
// code for editor.h

#include "editor.h"          // this module
#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine
#include "xassert.h"         // xassert
#include "array.h"           // Array
#include "style.h"           // LineStyle, etc.

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
    // cursor, select and visible line/col inited by resetView()
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    cursorColor(0x00, 0xFF, 0xFF),  // cyan
    normalFG(0xFF, 0xFF, 0xFF),     // white
    normalBG(0x00, 0x00, 0xA0),     // darkish blue
    selectFG(0xFF, 0xFF, 0xFF),     // white
    selectBG(0x00, 0x00, 0xF0),     // light blue
    ctrlShiftDistance(10)
    // font metrics inited by setFont()
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);

  setCursor(ibeamCursor);

  // use the color scheme for text widgets; typically this means
  // a white background, instead of a gray background
  //setBackgroundMode(PaletteBase);

  // fixed color
  setBackgroundColor(normalBG);

  resetView();
}


void Editor::resetView()
{
  cursorLine = 0;
  cursorCol = 0;
  selectLine = 0;
  selectCol = 0;
  selectEnabled = false;
  firstVisibleLine = 0;
  firstVisibleCol = 0;
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

  // buffer for text to print
  int visibleCols = lastVisibleCol - firstVisibleCol + 2;
  Array<char> text(visibleCols);

  // character style info
  LineStyle styles(0 /*normal*/);

  // currently selected style (so we can avoid possibly expensive
  // calls to change styles)
  int currentStyle = 0 /*normal*/;
  paint.setPen(normalFG);
  paint.setBackgroundColor(normalBG);

  // stats on cursor/selection
  int selLowLine, selLowCol;    // whichever of cursor/select comes first
  int selHighLine, selHighCol;  // whichever comes second
  if (cursorBeforeSelect()) {
    selLowLine = cursorLine;
    selLowCol = cursorCol;
    selHighLine = selectLine;
    selHighCol = selectCol;
  }
  else {
    selLowLine = selectLine;
    selLowCol = selectCol;
    selHighLine = cursorLine;
    selHighCol = cursorCol;
  }

  // paint the window
  for (int line = firstVisibleLine; y < height(); line++) {
    // fill the text with spaces, as the nominal text to display
    memset(text, ' ', visibleCols);

    // fill with text from the buffer
    if (line < buffer->numLines()) {
      int lineLen = buffer->lineLength(line);
      if (firstVisibleCol < lineLen) {
        int amt = min(lineLen - firstVisibleCol, visibleCols);
        buffer->getLine(line, firstVisibleCol, text, amt);
      }
    }

    // nominally the entire line is normal text
    styles.clear(0 /*normal*/);

    // incorporate effect of selection
    if (selectEnabled &&
        selLowLine <= line && line <= selHighLine) {
      if (selLowLine < line && line < selHighLine) {
        // entire line is selected
        styles.overlay(0, 0 /*infinite*/, 1 /*selected*/);
      }
      else if (selLowLine < line && line == selHighLine) {
        // first half of line is selected
        styles.overlay(0, selHighCol, 1 /*selected*/);
      }
      else if (selLowLine == line && line < selHighLine) {
        // right half of line is selected
        styles.overlay(selLowCol, 0 /*infinite*/, 1 /*selected*/);
      }
      else if (selLowLine == line && line == selHighLine) {
        // middle part of line is selected
        styles.overlay(selLowCol, selHighCol-selLowCol, 1 /*selected*/);
      }
      else {
        xfailure("messed up my logic");
      }
    }

    // next style entry to use
    LineStyleIter style(styles);
    style.advanceChars(firstVisibleCol);

    // right edge of what has not been painted
    int x = leftMargin;

    // number of characters printed
    int printed = 0;

    // loop over segments with different styles
    while (x < width()) {
      xassert(printed < visibleCols);

      // compute how many characters to print in this segment
      int len = style.length;
      if (style.length == 0) {
        // actually means infinite length
        len = visibleCols;
      }
      len = min(len, visibleCols-printed);
      xassert(len > 0);

      // set style
      if (style.style != currentStyle) {
        if (style.style == 0) {
          // normal
          paint.setPen(normalFG);
          paint.setBackgroundColor(normalBG);
        }
        else if (style.style == 1) {
          // selected
          paint.setPen(selectFG);
          paint.setBackgroundColor(selectBG);
        }
        else {
          xfailure("bad style code");
        }
        currentStyle = style.style;
      }

      // draw text; unfortunately this requires making two copies, one
      // to get a NUL-terminated source (could be avoided by
      // temporarily overwriting a character in 'text'), and one to
      // make a QString for drawText() to use; I think Qt should have
      // made drawText() accept an ordinary char const* ...
      string segment(text+printed, len);
      paint.drawText(x, y+ascent-1,               // baseline start coordinate
                     QString(segment), len);      // text, length

      // advance
      x += fontWidth * len;
      printed += len;
    }

    // draw the cursor as a line
    if (line == cursorLine) {
      paint.save();

      paint.setPen(cursorColor);
      x = leftMargin + fontWidth * (cursorCol - firstVisibleCol);
      paint.drawLine(x,y, x, y+fontHeight-1);
      paint.drawLine(x-1,y, x-1, y+fontHeight-1);

      paint.restore();
    }

    y += fontHeight;

    if (interLineSpace > 0) {
      // I haven't tested this code...
      paint.eraseRect(0, y, width(), interLineSpace);
      y += interLineSpace;
    }
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

  // <key> and shift-<key>
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


bool Editor::cursorBeforeSelect() const
{
  if (cursorLine < selectLine) return true;
  if (cursorLine > selectLine) return false;
  return cursorCol < selectCol;
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
