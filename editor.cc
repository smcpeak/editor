// editor.cc
// code for editor.h

#include "editor.h"          // this module
#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine
#include "xassert.h"         // xassert
#include "array.h"           // Array
#include "style.h"           // LineStyle, etc.
#include "trace.h"           // TRACE
#include "qtutil.h"          // toString(QKeyEvent&)
#include "macros.h"          // Restorer
#include "styledb.h"         // StyleDB
#include "inputproxy.h"      // InputProxy

#include <qapplication.h>    // QApplication
#include <qpainter.h>        // QPainter
#include <qfontmetrics.h>    // QFontMetrics
#include <qrangecontrol.h>   // QRangeControl
#include <qpixmap.h>         // QPixmap
#include <qmessagebox.h>     // QMessageBox
#include <qclipboard.h>      // QClipboard
#include <qlabel.h>          // QLabel

#include <stdio.h>           // printf, for debugging


// ---------------------- Editor --------------------------------
Editor::Editor(BufferState *buf,
               QWidget *parent=NULL, const char *name=NULL)
  : QWidget(parent, name, WRepaintNoErase | WResizeNoErase | WNorthWestGravity),
    EditingState(),
    infoBox(NULL),
    buffer(buf),
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    cursorColor(0x00, 0xFF, 0xFF),  // cyan
    // fonts set by setFont()
    ctrlShiftDistance(10),
    inputProxy(NULL),
    // font metrics inited by setFont()
    ignoreScrollSignals(false)
{
  QFont font;
  font.setRawName("-scott-editor-medium-r-normal--14-140-75-75-m-90-iso8859-1");
  setFont(font);

  setCursor(ibeamCursor);

  // use the color scheme for text widgets; typically this means
  // a white background, instead of a gray background
  //setBackgroundMode(PaletteBase);

  // needed to cause Qt not to erase window?
  setBackgroundMode(NoBackground);

  // fixed color
  //setBackgroundColor(normalBG);

  resetView();
}

Editor::~Editor()
{
  if (inputProxy) {
    inputProxy->detach();
  }
}


void Editor::cursorTo(int line, int col)
{
  buffer->moveAbsCursor(line, col);
}

void Editor::resetView()
{
  cursorTo(0, 0);
  selectLine = 0;
  selectCol = 0;
  selectEnabled = false;
  selLowLine = selLowCol = selHighLine = selHighCol = 0;   // make it deterministic..
  setView(0,0);
}


bool Editor::cursorBeforeSelect() const
{
  if (cursorLine() < selectLine) return true;
  if (cursorLine() > selectLine) return false;
  return cursorCol() < selectCol;
}


void Editor::normalizeSelect()
{
  if (cursorBeforeSelect()) {
    selLowLine = cursorLine();
    selLowCol = cursorCol();
    selHighLine = selectLine;
    selHighCol = selectCol;
  }
  else {
    selLowLine = selectLine;
    selLowCol = selectCol;
    selHighLine = cursorLine();
    selHighCol = cursorCol();
  }
}


void Editor::setFont(QFont &f)
{
  normalFont = f;

  italicFont = f;
  italicFont.setItalic(true);

  boldFont = f;
  boldFont.setBold(true);

  // I might print the raw names to verify the system is using the
  // predefined variants, instead of, say, synthesizing its own
  TRACE("fontNames", ""
    << "\n  normal: " << (char const*)normalFont.rawName()
    << "\n  italic: " << (char const*)italicFont.rawName()
    << "\n  bold  : " << (char const*)boldFont.rawName());

  // I don't use setUnderline() because I'm a little suspicious of
  // what that really does... might experiment some..

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


void Editor::setBuffer(BufferState *buf)
{                          
  // save current editing state in current 'buffer'
  if (buffer) {     // allow initial buffer to be NULL
    buffer->savedState.copy(*this);
  }

  // switch to the new buffer, and retrieve its editing state
  buffer = buf;
  EditingState::copy(buf->savedState);

  redraw();
}


void Editor::redraw()
{
  updateView();
  
  // tell our parent.. but ignore certain messages temporarily
  {
    Restorer<bool> restore(ignoreScrollSignals, true);
    emit viewChanged();
  }
  
  // redraw
  update();

  // I think that update() should work, but it does not: it erases
  // the window first.  I've tried lots of different things to fix
  // it, but can't find the solution.  repaint(false) does seem to
  // work, however
  //repaint(false /*erase*/);
  
  // update: It turns out that update() and repaint(false) *were*
  // doing the same thing, but that the PaintEvent::erase flag was
  // simply wrong.  Moreover, the flicker was actually server-side,
  // due to a bug in XFree86's implementation of XDrawImageSting.
  // So I've switched to a server-side pixmap double-buffer method,
  // which of course eliminates the flickering.
}


void Editor::setView(int newFirstLine, int newFirstCol)
{
  xassert(newFirstLine >= 0);
  xassert(newFirstCol >= 0);

  if (newFirstLine==firstVisibleLine &&
      newFirstCol==firstVisibleCol) {
    // nop
  }
  else {
    setFirstVisibleLC(newFirstLine, newFirstCol);

    updateView();

    TRACE("setView", "new firstVis is " << firstVisStr());
  }
}


void Editor::moveView(int deltaLine, int deltaCol)
{
  int line = max(0, firstVisibleLine + deltaLine);
  int col = max(0, firstVisibleCol + deltaCol);

  setView(line, col);
}


void Editor::updateView()
{
  int h = height();
  int w = width();

  if (fontHeight && fontWidth) {
    // calculate viewport stats
    // why -1?  suppose width==height==0, then the "first" visible isn't
    // visible at all, so we'd want the one before (not that that's visible
    // either, but it suggests what we want in nondegenerate cases too)
    lastVisibleLine = firstVisibleLine
      + (h - topMargin) / lineHeight() - 1;
    lastVisibleCol = firstVisibleCol
      + (w - leftMargin) / fontWidth - 1;
  }
  else {
    // font info not set, leave them alone
  }
}


void Editor::resizeEvent(QResizeEvent *r)
{
  QWidget::resizeEvent(r);
  updateView();
  emit viewChanged();
}

 
// for calling from gdb..
int flushPainter(QPainter &p)
{
  p.flush();
  return 0;
}

  
// In general, to avoid flickering, I try to paint every pixel
// exactly once (this idea comes straight from the Qt tutorial).
// So far, the only place I violate that is the cursor, the pixels
// of which are drawn twice when it is visible.
// UPDATE: It's irrelevant now that I've been forced into double-
// buffering by a bug in XFree86 (see redraw()).
void Editor::paintEvent(QPaintEvent *ev)
{
  // testing... it turns out this flag is not accurate, because
  // when the PaintEvent is dispatched a new PaintEvent object
  // is created, and that one doesn't have the 'erase' flag set
  // properly, even though in fact no erasing was done
  #if 0
  if (ev->erased()) {
    printf("erased! noerase:%d\n",
           (int)testWFlags(WRepaintNoErase));
  }
  #endif // 0/1

  // make a pixmap, so as to avoid flickering by double-buffering
  QPixmap pixmap(size());

  // set up the painter; we have to copy over some settings explicitly
  QPainter paint(&pixmap);
  paint.setFont(font());

  // draw on the pixmap
  drawBufferContents(paint);
  paint.end();

  // blit the pixmap.. Qt docs claim the pixmap is entirely server-side,
  // so this should *not* entail a network copy of a big image...
  paint.begin(this);
  paint.drawPixmap(0,0, pixmap);
}

void Editor::drawBufferContents(QPainter &paint)
{
  // when drawing text, erase background automatically
  paint.setBackgroundMode(OpaqueMode);

  // character style info
  LineStyle styles(ST_NORMAL);

  // currently selected style (so we can avoid possibly expensive
  // calls to change styles)
  Style currentStyle = ST_NORMAL;
  bool underlining = false;     // whether drawing underlines
  StyleDB *styleDB = StyleDB::instance();
  setDrawStyle(paint, underlining, styleDB, currentStyle);

  // it turns out my 'editor' font has a bold variant
  // with one pixel less of descent.. this causes lines to be incompletely
  // erased.. so for now, erase the whole thing in advance (HACK!)
  paint.eraseRect(0,0, width(),height());

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
  xassert(lineHeight() > 0);

  // buffer for text to print
  int visibleCols = lastVisibleCol - firstVisibleCol + 2;
  Array<char> text(visibleCols);

  // set sel{Low,High}{Line,Col}
  normalizeSelect();

  // paint the window
  for (int line = firstVisibleLine; y < height(); line++) {
    // fill the text with spaces, as the nominal text to display;
    // these will only be used if there is style information out
    // beyond the actual line character data
    memset(text, ' ', visibleCols);

    // number of characters from this line that are visible
    int visibleLineChars = 0;

    // nominally the entire line is normal text
    styles.clear(ST_NORMAL);

    // fill with text from the buffer
    if (line < buffer->numLines()) {
      int lineLen = buffer->lineLength(line);
      if (firstVisibleCol < lineLen) {
        int amt = min(lineLen - firstVisibleCol, visibleCols);
        buffer->getLine(line, firstVisibleCol, text, amt);
        visibleLineChars = amt;
      }

      // apply highlighting
      if (buffer->highlighter) {
        buffer->highlighter->highlight(buffer->core(), line, styles);
      }
    }
    xassert(visibleLineChars <= visibleCols);

    // show hits
    if (hitText.length() > 0) {
      int hitLine=line, hitCol=0;
      while (buffer->findString(hitLine, hitCol, hitText, 
                                (hitTextFlags | Buffer::FS_ONE_LINE))) {
        styles.overlay(hitCol, hitText.length(), ST_HITS);
        hitCol++;
      }
    }

    // incorporate effect of selection
    if (selectEnabled &&
        selLowLine <= line && line <= selHighLine) {
      if (selLowLine < line && line < selHighLine) {
        // entire line is selected
        styles.overlay(0, 0 /*infinite*/, ST_SELECTION);
      }
      else if (selLowLine < line && line == selHighLine) {
        // first half of line is selected
        if (selHighCol) {
          styles.overlay(0, selHighCol, ST_SELECTION);
        }
      }
      else if (selLowLine == line && line < selHighLine) {
        // right half of line is selected
        styles.overlay(selLowCol, 0 /*infinite*/, ST_SELECTION);
      }
      else if (selLowLine == line && line == selHighLine) {
        // middle part of line is selected
        if (selHighCol != selLowCol) {
          styles.overlay(selLowCol, selHighCol-selLowCol, ST_SELECTION);
        }
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

      // set style
      if (style.style != currentStyle) {
        currentStyle = style.style;
        setDrawStyle(paint, underlining, styleDB, currentStyle);
      }

      // compute how many characters to print in this segment
      int len = style.length;
      if (style.length == 0) {
        // actually means infinite length
        if (printed >= visibleLineChars) {
          // we've printed all the interesting characters on this line
          // because we're past the end of the line's chars, and we're
          // one the last style run; for efficiency of communication
          // with the X server, render the remainder of this line with
          // a single rectangle
          paint.eraseRect(x,y, width()-x, fontHeight);
          break;   // out of loop over line segments
        }

        // print only the remaining chars on the line, to improve
        // the chances we'll use the eraseRect() optimization above
        len = visibleLineChars-printed;
      }
      len = min(len, visibleCols-printed);
      xassert(len > 0);

      // draw text; unfortunately this requires making two copies, one
      // to get a NUL-terminated source (could be avoided by
      // temporarily overwriting a character in 'text'), and one to
      // make a QString for drawText() to use; I think Qt should have
      // made drawText() accept an ordinary char const* ...
      string segment(text+printed, len);
      int baseline = y+ascent-1;
      paint.drawText(x, baseline,                 // baseline start coordinate
                     QString(segment), len);      // text, length

      if (underlining) {
        // want to draw a line on top of where underscores would be; this
        // might not be consistent across fonts, so I might want to have
        // a user-specifiable underlining offset.. also, I don't want this
        // going into the next line, so truncate according to descent
        baseline += min(2 /*nominal underline offset*/, descent);
        paint.drawLine(x, baseline, x + fontWidth*len, baseline);
      }

      // advance
      x += fontWidth * len;
      printed += len;
      style.advanceChars(len);
    }

    // draw the cursor as a line
    if (line == cursorLine()) {
      paint.save();

      paint.setPen(cursorColor);
      x = leftMargin + fontWidth * (cursorCol() - firstVisibleCol);
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


void Editor::setDrawStyle(QPainter &paint, bool &underlining,
                          StyleDB *db, Style s)
{
  TextStyle const &ts = db->getStyle(s);
  paint.setPen(ts.foreground);
  paint.setBackgroundColor(ts.background);
  underlining = false;
  switch (ts.variant) {
    default: xfailure("bad variant code");
    case FV_UNDERLINE:  underlining = true;   // drop through to next
    case FV_NORMAL:     paint.setFont(normalFont);  break;
    case FV_ITALIC:     paint.setFont(italicFont);  break;
    case FV_BOLD:       paint.setFont(boldFont);    break;
  }
}


// increment, but don't allow result to go below 0
static void inc(int &val, int amt)
{
  val += amt;
  if (val < 0) {
    val = 0;
  }
}


void Editor::cursorToTop()
{
  cursorTo(0, 0);
  scrollToCursor();
}

void Editor::cursorToBottom()
{
  cursorTo(max(buffer->numLines()-1,0), 0);
  scrollToCursor();
  redraw();
}


void Editor::turnOffSelection()
{
  selectEnabled = false;
}

void Editor::turnOnSelection()
{
  if (!selectEnabled) {
    selectLine = cursorLine();
    selectCol = cursorCol();
    selectEnabled = true;
  }
}

void Editor::turnSelection(bool on)
{
  if (on) {
    turnOnSelection();
  }
  else {
    turnOffSelection();
  }
}

void Editor::clearSelIfEmpty()
{
  if (selectEnabled &&
      cursorLine()==selectLine &&
      cursorCol()==selectCol) {
    turnOffSelection();
  }
}


void Editor::keyPressEvent(QKeyEvent *k)
{
  TRACE("input", "keyPress: " << toString(*k));

  if (inputProxy && inputProxy->keyPressEvent(k)) {
    return;
  }

  int state = k->state() & KeyButtonMask;

  // Ctrl+<key>
  if (state == ControlButton) {
    switch (k->key()) {
      case Key_Insert:
        editCopy();
        break;

      case Key_U:
        buffer->core().dumpRepresentation();
        break;

      case Key_PageUp:
        turnOffSelection();
        cursorToTop();
        break;

      case Key_PageDown:
        turnOffSelection();
        cursorToBottom();
        break;

      case Key_W:
        moveView(-1, 0);
        if (cursorLine() > lastVisibleLine) {
          cursorUpBy(cursorLine() - lastVisibleLine);
        }
        redraw();
        break;

      case Key_Z:
        moveView(+1, 0);
        if (cursorLine() < firstVisibleLine) {
          cursorDownBy(firstVisibleLine - cursorLine());
        }
        redraw();
        break;

      case Key_Up:
        moveViewAndCursor(-1, 0);
        break;

      case Key_Down:
        moveViewAndCursor(+1, 0);
        break;

      case Key_Left:
        moveViewAndCursor(0, -1);
        break;

      case Key_Right:
        moveViewAndCursor(0, +1);
        break;

      case Key_B:      cursorLeft(false); break;
      case Key_F:      cursorRight(false); break;
      case Key_A:      cursorHome(false); break;
      case Key_E:      cursorEnd(false); break;
      case Key_P:      cursorUp(false); break;
      case Key_N:      cursorDown(false); break;
      // emacs' pageup/pagedown are ctrl-v and alt-v, but the
      // latter should be reserved for accessing the menu, so I'm
      // not going to bind either by default

      case Key_D:      deleteCharAtCursor(); break;

      case Key_L:
        setView(max(0, cursorLine() - visLines()/2), 0);
        scrollToCursor();
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Alt+<key>
  else if (state == AltButton) {
    switch (k->key()) {
      case Key_Left:
        blockIndent(-2);
        break;
        
      case Key_Right:
        blockIndent(+2);
        break;
    }
  }

  // Ctrl+Alt+<key>
  else if (state == (ControlButton|AltButton)) {
    switch (k->key()) {
      #if 0     // moved into EditorWindow class
      case Key_Left: {
        QWidget *top = qApp->mainWidget();
        top->setGeometry(83, 55, 565, 867);
        break;
      }

      case Key_Right: {
        QWidget *top = qApp->mainWidget();
        top->setGeometry(493, 55, 781, 867);
        break;
      }                                         
      #endif // 0

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
        moveViewAndCursor(-ctrlShiftDistance, 0);
        break;

      case Key_Down:
        moveViewAndCursor(+ctrlShiftDistance, 0);
        break;

      case Key_Left:
        moveViewAndCursor(0, -ctrlShiftDistance);
        break;

      case Key_Right:
        moveViewAndCursor(0, +ctrlShiftDistance);
        break;

      case Key_PageUp:
        turnOnSelection();
        cursorToTop();
        break;

      case Key_PageDown:
        turnOnSelection();
        cursorToBottom();
        break;
       
      case Key_B:      cursorLeft(true); break;
      case Key_F:      cursorRight(true); break;
      case Key_A:      cursorHome(true); break;
      case Key_E:      cursorEnd(true); break;
      case Key_P:      cursorUp(true); break;
      case Key_N:      cursorDown(true); break;

      default:
        k->ignore();
        break;
    }
  }

  // <key> and shift-<key>
  else if (state == NoButton || state == ShiftButton) {
    switch (k->key()) {
      case Key_Insert:
        if (state == ShiftButton) {
          editPaste();
        }
        else {
          // TODO: toggle insert/overwrite mode
        }
        break;

      case Key_Left:     cursorLeft(state==ShiftButton); break;
      case Key_Right:    cursorRight(state==ShiftButton); break;
      case Key_Home:     cursorHome(state==ShiftButton); break;
      case Key_End:      cursorEnd(state==ShiftButton); break;
      case Key_Up:       cursorUp(state==ShiftButton); break;
      case Key_Down:     cursorDown(state==ShiftButton); break;
      case Key_PageUp:   cursorPageUp(state==ShiftButton); break;
      case Key_PageDown: cursorPageDown(state==ShiftButton); break;

      case Key_BackSpace: {
        fillToCursor();
        buffer->changed = true;

        if (selectEnabled) {
          editDelete();
        }
        else if (cursorCol() == 0) {
          if (cursorLine() == 0) {
            // do nothing
          }
          else {
            // move to end of previous line
            buffer->moveToPrevLineEnd();

            // splice them together
            spliceNextLine();
          }
        }
        else {
          // remove the character to the left of the cursor
          buffer->deleteLR(true /*left*/, 1);
        }

        scrollToCursor();
        break;
      }

      case Key_Delete: {
        if (state == NoButton) {
          deleteCharAtCursor();
        }
        else {   // shift-delete
          editCut();
        }
        break;
      }

      case Key_Enter:
      case Key_Return: {
        fillToCursor();
        buffer->changed = true;

        // typing replaces selection
        if (selectEnabled) {
          editDelete();
        }

        buffer->insertNewline();

        // make sure we can see as much to the left as possible
        setFirstVisibleCol(0);

        // auto-indent
        int ind = buffer->getAboveIndentation(cursorLine()-1);
        buffer->insertSpaces(ind);

        scrollToCursor();
        break;
      }

      case Key_Escape:
        // do nothing; in other modes this will cancel out
        
        // well, almost nothing
        hitText = "";
        redraw();
        break;

      default: {
        QString text = k->text();
        if (text.length() && text[0].isPrint()) {
          fillToCursor();
          buffer->changed = true;

          // typing replaces selection
          if (selectEnabled) {
            editDelete();
          }
          // insert this character at the cursor
          buffer->insertLR(false /*left*/, text, text.length());
          scrollToCursor();
        }
        else {
          k->ignore();
          return;
        }
      }
    }
  }

  // other combinations
  else {
    k->ignore();
  }
}


void Editor::insertAtCursor(char const *text)
{
  buffer->changed = true;
  buffer->insertText(text);
  scrollToCursor();
}

void Editor::deleteAtCursor(int amt)
{
  xassert(amt >= 0);
  if (amt == 0) {
    return;
  }

  fillToCursor();
  buffer->deleteLR(true /*left*/, amt);
  buffer->changed = true;
  scrollToCursor();
}


void Editor::fillToCursor()
{
  buffer->fillToCursor();
}


void Editor::spliceNextLine()
{
  // cursor must be at the end of a line
  xassert(cursorCol() == buffer->lineLength(cursorLine()));

  buffer->deleteChar();
}


void Editor::scrollToCursor(int edgeGap)
{
  int fvline = firstVisibleLine;
  int fvcol = firstVisibleCol;

  if (cursorCol()-edgeGap < firstVisibleCol) {
    fvcol = max(0, cursorCol()-edgeGap);
  }
  else if (cursorCol()+edgeGap > lastVisibleCol) {
    fvcol += (cursorCol()+edgeGap - lastVisibleCol);
  }

  if (cursorLine()-edgeGap < firstVisibleLine) {
    fvline = max(0, cursorLine()-edgeGap);
  }
  else if (cursorLine()+edgeGap > lastVisibleLine) {
    fvline += (cursorLine()+edgeGap - lastVisibleLine);
  }
  
  setView(fvline, fvcol);
  
  redraw();
}


STATICDEF string Editor::lineColStr(int line, int col)
{
  return stringc << line << ":" << col;
}

void Editor::moveViewAndCursor(int deltaLine, int deltaCol)
{
  TRACE("moveViewAndCursor", 
        "start: firstVis=" << firstVisStr()
     << ", cursor=" << cursorStr()
     << ", delta=" << lineColStr(deltaLine, deltaCol));

  // first make sure the view contains the cursor
  scrollToCursor();

  // move viewport, but remember original so we can tell
  // when there's truncation
  int origVL = firstVisibleLine;
  int origVC = firstVisibleCol;
  moveView(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  moveCursorBy(firstVisibleLine - origVL,
               firstVisibleCol - origVC);

  TRACE("moveViewAndCursor",
        "end: firstVis=" << firstVisStr() <<
        ", cursor=" << cursorStr());

  // redraw display
  redraw();
}


void Editor::scrollToLine(int line)
{
  if (!ignoreScrollSignals) {
    xassert(line >= 0);
    setFirstVisibleLine(line);
    redraw();
  }
}

void Editor::scrollToCol(int col)
{  
  if (!ignoreScrollSignals) {
    xassert(col >= 0);
    setFirstVisibleCol(col);
    redraw();
  }
}


void Editor::setCursorToClickLoc(QMouseEvent *m)
{
  int x = m->x();
  int y = m->y();

  // subtract off the margin, but don't let either coord go negative
  inc(x, -leftMargin);
  inc(y, -topMargin);

  int newLine = y/lineHeight() + firstVisibleLine;
  int newCol = x/fontWidth + firstVisibleCol;

  //printf("click: (%d,%d)     goto line %d, col %d\n",
  //       x, y, newLine, newCol);

  cursorTo(newLine, newCol);

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
}


void Editor::mousePressEvent(QMouseEvent *m)
{
  // get rid of popups?
  QWidget::mousePressEvent(m);

  turnOffSelection();
  setCursorToClickLoc(m);

  redraw();
}


void Editor::mouseMoveEvent(QMouseEvent *m)
{
  QWidget::mouseMoveEvent(m);

  turnOnSelection();
  setCursorToClickLoc(m);
  clearSelIfEmpty();

  redraw();
}


void Editor::mouseReleaseEvent(QMouseEvent *m)
{
  QWidget::mouseReleaseEvent(m);

  turnOnSelection();
  setCursorToClickLoc(m);
  clearSelIfEmpty();

  redraw();
}


// ----------------------- edit menu -----------------------
void Editor::editCut()
{
  if (selectEnabled) {
    editCopy();
    selectEnabled = true;    // counteract something editCopy() does
    editDelete();
  }
}


void Editor::editCopy()
{
  if (selectEnabled) {
    // get selected text
    string sel = getSelectedText();

    // put it into the clipboard
    QClipboard *cb = QApplication::clipboard();
    cb->setText(QString(sel));
    
    // un-highlight the selection, which is what emacs does and
    // I'm now used to
    selectEnabled = false;
    redraw();
  }
}


void Editor::editPaste()
{
  // get contents of clipboard
  QClipboard *cb = QApplication::clipboard();
  QString text = cb->text();
  if (!text) {
    QMessageBox::information(this, "Info", "The clipboard is empty.");
  }
  else {
    fillToCursor();

    // remove what's selected, if anything
    editDelete();

    // insert at cursor
    insertAtCursor(text);
  }
}


void Editor::editDelete()
{
  if (selectEnabled) {
    normalizeSelect();
    buffer->changed = true;
    buffer->deleteTextRange(selLowLine, selLowCol, selHighLine, selHighCol);

    selectEnabled = false;
    scrollToCursor();
  }
}


void Editor::showInfo(char const *infoString)
{
  QWidget *main = qApp->mainWidget();

  if (!infoBox) {
    infoBox = new QLabel(main, "infoBox",
      // style of a QTipLabel for tooltips ($QTDIR/src/widgets/qtooltip.cpp)
      WStyle_Customize + WStyle_NoBorder + WStyle_Tool);

    infoBox->setBackgroundColor(QColor(0xFF,0xFF,0x80));
  }

  infoBox->setText(infoString);

  // compute a good size for the label
  QFontMetrics fm(infoBox->font());
  QSize sz = fm.size(0, infoString);
  infoBox->resize(sz.width() + 2, sz.height() + 2);

  infoBox->move(mapTo(main,
    QPoint((cursorCol() - firstVisibleCol) * fontWidth,
           (cursorLine() - firstVisibleLine + 1) * fontHeight + 1)));
           
  // try to position the box inside the main widget, so it will show up
  if (infoBox->x() + infoBox->width() > main->width()) {
    infoBox->move(main->width() - infoBox->width(), infoBox->y());
  }

  infoBox->show();
}

void Editor::hideInfo()
{
  if (infoBox) {
    delete infoBox;
    infoBox = NULL;
  }
}
                    

void Editor::cursorLeft(bool shift)
{
  turnSelection(shift);
  cursorLeftBy(1);
  scrollToCursor();
}

void Editor::cursorRight(bool shift)
{
  turnSelection(shift);
  cursorRightBy(1);
  scrollToCursor();
}

void Editor::cursorHome(bool shift)
{
  turnSelection(shift);
  buffer->moveAbsColumn(0);
  scrollToCursor();
}

void Editor::cursorEnd(bool shift)
{
  turnSelection(shift);
  buffer->moveAbsColumn(buffer->lineLength(cursorLine()));
  scrollToCursor();
}

void Editor::cursorUp(bool shift)
{
  turnSelection(shift);
  cursorUpBy(1);
  scrollToCursor();
}

void Editor::cursorDown(bool shift)
{
  // allows cursor past EOF..
  turnSelection(shift);
  cursorDownBy(1);
  scrollToCursor();
}

void Editor::cursorPageUp(bool shift)
{
  turnSelection(shift);
  moveViewAndCursor(-visLines(), 0);
}

void Editor::cursorPageDown(bool shift)
{
  turnSelection(shift);
  moveViewAndCursor(+visLines(), 0);
}


void Editor::deleteCharAtCursor()
{
  fillToCursor();
  buffer->changed = true;

  if (selectEnabled) {
    editDelete();
  }
  else if (cursorCol() == buffer->lineLength(cursorLine())) {
    // splice next line onto this one
    spliceNextLine();
  }
  else /* cursor < lineLength */ {
    // delete character to right of cursor
    buffer->deleteText(1);
  }

  scrollToCursor();
}


void Editor::blockIndent(int amt)
{
  if (!selectEnabled) {
    return;      // nop
  }
  
  normalizeSelect();              
  
  int endLine = (selHighCol==0? selHighLine-1 : selHighLine);
  endLine = min(endLine, buffer->numLines()-1);
  buffer->indentLines(selLowLine, endLine-selLowLine+1, amt);
  
  redraw();
}


string Editor::getSelectedText()
{
  if (!selectEnabled) {
    return "";
  }
  else {
    normalizeSelect();   // this is why this method is not 'const' ...
    return buffer->getTextRange(selLowLine, selLowCol, selHighLine, selHighCol);
  }
}


// EOF
