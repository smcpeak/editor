// editor.cc
// code for editor.h

#include "editor.h"          // this module

// this dir
#include "buffer.h"          // Buffer
#include "inputproxy.h"      // InputProxy
#include "position.h"        // Position
#include "qtbdffont.h"       // QtBDFFont
#include "qtutil.h"          // toString(QKeyEvent&)
#include "status.h"          // StatusDisplay
#include "styledb.h"         // StyleDB
#include "textcategory.h"    // LineCategories, etc.
#include "textline.h"        // TextLine

// default font definitions (in smqtutil directory)
#include "editor14r.bdf.gen.h"
#include "editor14i.bdf.gen.h"
#include "editor14b.bdf.gen.h"
#include "minihex6.bdf.gen.h"          // bdfFontData_minihex6


// smbase
#include "array.h"           // Array
#include "bdffont.h"         // BDFFont
#include "ckheap.h"          // malloc_stats
#include "macros.h"          // Restorer
#include "nonport.h"         // getMilliseconds
#include "trace.h"           // TRACE
#include "xassert.h"         // xassert

// Qt
#include <qapplication.h>    // QApplication
#include <qclipboard.h>      // QClipboard
#include <qfontmetrics.h>    // QFontMetrics
#include <qlabel.h>          // QLabel
#include <qmessagebox.h>     // QMessageBox
#include <qpainter.h>        // QPainter
#include <qpixmap.h>         // QPixmap

#include <QKeyEvent>
#include <QPaintEvent>

// libc
#include <stdio.h>           // printf, for debugging
#include <time.h>            // time(), localtime()


// Distance below the baseline to draw an underline.
int const UNDERLINE_OFFSET = 2;


// ---------------------- Editor --------------------------------
int Editor::objectCount = 0;

Editor::Editor(BufferState *buf, StatusDisplay *stat,
               QWidget *parent)
  : QWidget(parent),
    SavedEditingState(),
    infoBox(NULL),
    status(stat),
    buffer(buf),
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    cursorColor(0xFF, 0xFF, 0xFF),       // white
    fontForCategory(NUM_STANDARD_TEXT_CATEGORIES),
    cursorFontForFV(FV_BOLD + 1),
    minihexFont(),
    visibleWhitespace(true),
    whitespaceOpacity(32),
    ctrlShiftDistance(10),
    inputProxy(NULL),
    // font metrics inited by setFont()
    listening(false),
    nonfocusCursorLine(0), nonfocusCursorCol(0),
    ignoreScrollSignals(false)
{
  setFonts(bdfFontData_editor14r,
           bdfFontData_editor14i,
           bdfFontData_editor14b);

  setCursor(Qt::IBeamCursor);

  // required to accept focus
  setFocusPolicy(Qt::StrongFocus);

  resetView();

  Editor::objectCount++;
}

Editor::~Editor()
{
  Editor::objectCount--;
  if (inputProxy) {
    inputProxy->detach();
  }

  fontForCategory.deleteAll();
}


void Editor::cursorTo(int line, int col)
{
  buffer->moveAbsCursor(line, col);

  // set the nonfocus location too, in case the we happen to
  // not have the focus right now (e.g. the Alt+G dialog);
  // actually, the need for thisexposes the fact that my current
  // solution to nonfocus cursor issues isn't very good.. hmm...
  nonfocusCursorLine = line;
  nonfocusCursorCol = col;
}

void Editor::resetView()
{
  if (buffer) {
    cursorTo(0, 0);
  }
  this->selectLine = 0;
  this->selectCol = 0;
  this->selectEnabled = false;
  selLowLine = selLowCol = selHighLine = selHighCol = 0;   // make it deterministic..
  setView(0,0);
}


bool Editor::cursorBeforeSelect() const
{
  // TODO: I should create a class with line+col to better
  // encapsulate logic like this.
  if (cursorLine() < this->selectLine) return true;
  if (cursorLine() > this->selectLine) return false;
  return cursorCol() < this->selectCol;
}


void Editor::normalizeSelect(int cursorLine, int cursorCol)
{
  if (cursorBeforeSelect()) {
    selLowLine = cursorLine;
    selLowCol = cursorCol;
    selHighLine = this->selectLine;
    selHighCol = this->selectCol;
  }
  else {
    selLowLine = this->selectLine;
    selLowCol = this->selectCol;
    selHighLine = cursorLine;
    selHighCol = cursorCol;
  }
}


static BDFFont *makeBDFFont(char const *bdfData, char const *context)
{
  try {
    BDFFont *ret = new BDFFont;
    parseBDFString(*ret, bdfData);
    return ret;
  }
  catch (xBase &x) {
    x.prependContext(context);
    throw;
  }
}


void Editor::setFonts(char const *normal, char const *italic, char const *bold)
{
  // Read the font files, and index the results by FontVariant.
  ObjArrayStack<BDFFont> bdfFonts(3);
  STATIC_ASSERT(FV_NORMAL == 0);
  bdfFonts.push(makeBDFFont(normal, "normal font"));
  STATIC_ASSERT(FV_ITALIC == 1);
  bdfFonts.push(makeBDFFont(italic, "italic font"));
  STATIC_ASSERT(FV_BOLD == 2);
  bdfFonts.push(makeBDFFont(bold, "bold font"));

  // Using one fixed global style mapping.
  StyleDB *styleDB = StyleDB::instance();

  // Build the complete set of new fonts.
  {
    ObjArrayStack<QtBDFFont> newFonts(NUM_STANDARD_TEXT_CATEGORIES);
    for (int category = TC_ZERO; category < NUM_STANDARD_TEXT_CATEGORIES; category++) {
      TextStyle const &ts = styleDB->getStyle((TextCategory)category);

      STATIC_ASSERT(FV_BOLD == 2);
      BDFFont *bdfFont = bdfFonts[ts.variant % 3];

      QtBDFFont *qfont = new QtBDFFont(*bdfFont);
      qfont->setFgColor(ts.foreground);
      qfont->setBgColor(ts.background);
      qfont->setTransparent(false);
      newFonts.push(qfont);
    }

    // Substitute the new for the old.
    fontForCategory.swapWith(newFonts);
  }

  // Repeat the procedure for the cursor fonts.
  {
    ObjArrayStack<QtBDFFont> newFonts(FV_BOLD + 1);
    for (int fv = 0; fv <= FV_BOLD; fv++) {
      QtBDFFont *qfont = new QtBDFFont(*(bdfFonts[fv]));

      // The character under the cursor is drawn with the normal background
      // color, and the cursor box (its background) is drawn in 'cursorColor'.
      qfont->setFgColor(styleDB->getStyle(TC_NORMAL).background);
      qfont->setBgColor(cursorColor);
      qfont->setTransparent(false);

      newFonts.push(qfont);
    }

    cursorFontForFV.swapWith(newFonts);
  }

  // calculate metrics
  QRect const &bbox = fontForCategory[TC_NORMAL]->getAllCharsBBox();
  ascent = -bbox.top();
  descent = bbox.bottom() + 1;
  fontHeight = ascent + descent;
  xassert(fontHeight == bbox.height());    // check my assumptions
  fontWidth = bbox.width();

  // Font for missing glyphs.
  Owner<BDFFont> minihexBDFFont(makeBDFFont(bdfFontData_minihex6, "minihex font"));
  this->minihexFont = new QtBDFFont(*minihexBDFFont);
  this->minihexFont->setTransparent(false);
}


void Editor::setBuffer(BufferState *buf)
{
  bool wasListening = this->listening;
  if (wasListening) {
    this->stopListening();
  }

  // save current editing state in current 'buffer'
  if (this->buffer) {     // allow initial buffer to be NULL
    this->buffer->savedState.copySavedEditingState(*this);
  }

  // switch to the new buffer, and retrieve its editing state
  this->buffer = buf;
  this->copySavedEditingState(buf->savedState);

  if (wasListening) {
    this->startListening();
  }

  this->redraw();
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
}


void Editor::setView(int newFirstLine, int newFirstCol)
{
  xassert(newFirstLine >= 0);
  xassert(newFirstCol >= 0);

  if (newFirstLine == this->firstVisibleLine &&
      newFirstCol == this->firstVisibleCol) {
    // nop
  }
  else {
    this->setFirstVisibleLC(newFirstLine, newFirstCol);

    updateView();

    TRACE("setView", "new firstVis is " << firstVisStr());
  }
}


void Editor::moveView(int deltaLine, int deltaCol)
{
  int line = max(0, this->firstVisibleLine + deltaLine);
  int col = max(0, this->firstVisibleCol + deltaCol);

  this->setView(line, col);
}


void Editor::updateView()
{
  int h = this->height();
  int w = this->width();

  if (this->fontHeight && this->fontWidth) {
    // calculate viewport stats
    // why -1?  suppose width==height==0, then the "first" visible isn't
    // visible at all, so we'd want the one before (not that that's visible
    // either, but it suggests what we want in nondegenerate cases too)
    this->lastVisibleLine =
      this->firstVisibleLine +
      (h - this->topMargin) / this->lineHeight() - 1;
    this->lastVisibleCol =
      this->firstVisibleCol +
      (w - this->leftMargin) / this->fontWidth - 1;
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
  // This is what I did with Qt3:
  //p.flush();

  // This seems like perhaps it is the closest equivalent in Qt5.
  // But I won't be able to test it until I try debugging this on
  // Linux.
  p.beginNativePainting();
  p.endNativePainting();

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

  try {
    // draw on the pixmap
    if (!listening) {
      // usual case, draw cursor in usual location
      updateFrame(ev, cursorLine(), cursorCol());
    }
    else {
      // nonfocus synchronized update: use alternate location
      updateFrame(ev, nonfocusCursorLine, nonfocusCursorCol);
      TRACE("nonfocus", "drawing at " << nonfocusCursorLine <<
                        ":" << nonfocusCursorCol);
    }
  }
  catch (xBase &x) {
    // I can't pop up a message box because then when that
    // is dismissed it might trigger another exception, etc.
    QPainter paint(this);
    paint.setPen(Qt::white);
    paint.setBackgroundMode(Qt::OpaqueMode);
    paint.setBackground(Qt::red);
    paint.drawText(0, 30,                 // baseline start coordinate
                   toQString(x.why()));

    // Also write to stderr so rare issues can be seen.
    cerr << x.why() << endl;
  }
}

void Editor::updateFrame(QPaintEvent *ev, int cursorLine, int cursorCol)
{
  // debug info
  {
    string rect = "(none)";
    if (ev) {
      QRect const &r = ev->rect();
      rect = stringf("(%d,%d,%d,%d)", r.left(), r.top(),
                                      r.right(), r.bottom());
    }
    TRACE("paint", "frame: rect=" << rect);
  }

  // ---- setup painters ----
  // make a pixmap, so as to avoid flickering by double-buffering; the
  // pixmap is the entire width of the window, but only one line high,
  // so as to improve drawing locality and avoid excessive allocation
  // in the server
  int const lineWidth = width();
  int const fullLineHeight = fontHeight + interLineSpace;
  QPixmap pixmap(lineWidth, fullLineHeight);

  // NOTE: This does not preclude drawing objects that span multiple
  // lines, it just means that those objects need to be drawn one line
  // segment at a time.  i.e., the interface must have clients insert
  // objects into a display list, rather than drawing arbitrary things
  // on a canvas.

  // make the main painter, which will draw on the line pixmap; the
  // font setting must be copied over manually
  QPainter paint(&pixmap);
  paint.setFont(font());

  // Another painter will go to the window directly.  A key property
  // is that every pixel painted via 'winPaint' must be painted exactly
  // once, to avoid flickering.
  QPainter winPaint(this);

  // ---- setup style info ----
  // when drawing text, erase background automatically
  paint.setBackgroundMode(Qt::OpaqueMode);

  // Character style info.  This gets updated as we paint each line.
  LineCategories categories(TC_NORMAL);

  // Currently selected category and style (so we can avoid possibly
  // expensive calls to change styles).
  TextCategory currentCategory = TC_NORMAL;
  QtBDFFont *curFont = NULL;
  bool underlining = false;     // whether drawing underlines
  StyleDB *styleDB = StyleDB::instance();
  setDrawStyle(paint, curFont, underlining, styleDB, currentCategory);

  // do same for 'winPaint', just to set the background color
  setDrawStyle(winPaint, curFont, underlining, styleDB, currentCategory);

  // ---- margins ----
  // top edge of what has not been painted, in window coordinates
  int y = 0;

  if (topMargin) {
    winPaint.eraseRect(0, y, lineWidth, topMargin);
    y += topMargin;
  }

  // ---- remaining setup ----
  // Visible area info.  The +1 here is to include the column after
  // the last fully visible column, which might be partially visible.
  int const visibleCols = this->visCols() + 1;
  int const firstCol = this->firstVisibleCol;
  int const firstLine = this->firstVisibleLine;

  // I think it might be useful to support negative values for these
  // variables, but the code below is not prepared to deal with such
  // values at this time
  xassert(firstLine >= 0);
  xassert(firstCol >= 0);

  // another santiy check
  xassert(lineHeight() > 0);

  // Buffer that will be used for each visible line of text.
  Array<char> text(visibleCols);

  // set sel{Low,High}{Line,Col}
  normalizeSelect(cursorLine, cursorCol);

  // Paint the window, one line at a time.  Both 'line' and 'y' act
  // as loop control variables.
  for (int line = firstLine;
       y < this->height();
       line++, y += fullLineHeight)
  {
    // ---- compute style segments ----
    // fill the text with spaces, as the nominal text to display;
    // these will only be used if there is style information out
    // beyond the actual line character data
    memset(text, ' ', visibleCols);

    // number of characters from this line that are visible
    int visibleLineChars = 0;

    // nominally the entire line is normal text
    categories.clear(TC_NORMAL);

    // fill with text from the buffer
    if (line < buffer->numLines()) {
      // 1 if we will behave as though a newline character is
      // at the end of this line.
      int newlineAdjust = 0;
      if (this->visibleWhitespace && line < buffer->numLines()-1) {
        newlineAdjust = 1;
      }

      // Line length including possible synthesized newline.
      int const lineLen = buffer->lineLength(line) + newlineAdjust;

      if (firstCol < lineLen) {
        // First get the text without any extra newline.
        int const amt = min(lineLen-newlineAdjust - firstCol, visibleCols);
        buffer->getLine(line, firstCol, text, amt);
        visibleLineChars = amt;

        // Now possibly add the newline.
        if (visibleLineChars < visibleCols && newlineAdjust != 0) {
          text[visibleLineChars++] = '\n';
        }
      }

      // apply highlighting
      if (buffer->highlighter) {
        buffer->highlighter->highlight(buffer->core(), line, categories);
      }

      // Show search hits.
      if (this->hitText.length() > 0) {
        int hitLine = line;
        int hitCol = 0;
        Buffer::FindStringFlags const hitTextFlags =
          this->hitTextFlags | Buffer::FS_ONE_LINE;

        while (buffer->findString(hitLine /*INOUT*/, hitCol /*INOUT*/,
                                  toCStr(this->hitText),
                                  hitTextFlags)) {
          categories.overlay(hitCol, this->hitText.length(), TC_HITS);
          hitCol++;
        }
      }
    }
    xassert(visibleLineChars <= visibleCols);

    // incorporate effect of selection
    if (this->selectEnabled &&
        this->selLowLine <= line && line <= this->selHighLine)
    {
      if (selLowLine < line && line < selHighLine) {
        // entire line is selected
        categories.overlay(0, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selLowLine < line && line == selHighLine) {
        // first half of line is selected
        if (selHighCol) {
          categories.overlay(0, selHighCol, TC_SELECTION);
        }
      }
      else if (selLowLine == line && line < selHighLine) {
        // right half of line is selected
        categories.overlay(selLowCol, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selLowLine == line && line == selHighLine) {
        // middle part of line is selected
        if (selHighCol != selLowCol) {
          categories.overlay(selLowCol, selHighCol-selLowCol, TC_SELECTION);
        }
      }
      else {
        xfailure("messed up my logic");
      }
    }

    // Clear the left margin to the normal background color.
    if (currentCategory != TC_NORMAL) {
      currentCategory = TC_NORMAL;
      setDrawStyle(paint, curFont, underlining, styleDB, currentCategory);
    }
    paint.eraseRect(0,0, leftMargin, fullLineHeight);

    // Next category entry to use.
    LineCategoryIter category(categories);
    category.advanceChars(firstCol);

    // ---- render text+style segments -----
    // right edge of what has not been painted, relative to
    // the pixels in the pixmap
    int x = leftMargin;

    // number of characters printed
    int printed = 0;

    // 'y' coordinate of the origin point of characters
    int baseline = ascent-1;

    // loop over segments with different styles
    while (x < lineWidth) {
      xassert(printed < visibleCols);

      // set style
      if (category.category != currentCategory) {
        currentCategory = category.category;
        setDrawStyle(paint, curFont, underlining, styleDB, currentCategory);
      }

      // compute how many characters to print in this segment
      int len = category.length;
      if (category.length == 0) {
        // actually means infinite length
        if (printed >= visibleLineChars) {
          // we've printed all the interesting characters on this line
          // because we're past the end of the line's chars, and we're
          // one the last style run; for efficiency of communication
          // with the X server, render the remainder of this line with
          // a single rectangle
          paint.eraseRect(x,0, lineWidth-x, fullLineHeight);
          break;   // out of loop over line segments
        }

        // print only the remaining chars on the line, to improve
        // the chances we'll use the eraseRect() optimization above
        len = visibleLineChars-printed;
      }
      len = min(len, visibleCols-printed);
      xassert(len > 0);

      // The QtBDFFont package must be treated as if it draws
      // characters with transparency, even though the transparency
      // is only partial...
      paint.eraseRect(x,0, fontWidth*len, fullLineHeight);

      // draw text
      for (int i=0; i < len; i++) {
        this->drawOneChar(paint, curFont,
                          QPoint(x + fontWidth*i, baseline),
                          text[printed+i]);
      }

      if (underlining) {
        // want to draw a line on top of where underscores would be; this
        // might not be consistent across fonts, so I might want to have
        // a user-specifiable underlining offset.. also, I don't want this
        // going into the next line, so truncate according to descent
        int ulBaseline = baseline + min(UNDERLINE_OFFSET, descent);
        paint.drawLine(x, ulBaseline, x + fontWidth*len, ulBaseline);
      }

      // Advance to next category segment.
      x += fontWidth * len;
      printed += len;
      category.advanceChars(len);
    }

    // draw the cursor as a line
    if (line == cursorLine) {
      // just testing the mechanism that catches exceptions
      // raised while drawing
      //if (line == 5) {
      //  THROW(xBase("aiyee! sample exception!"));
      //}

      paint.save();

      // 0-based cursor column relative to what is visible
      int const visibleCursorCol = cursorCol - firstCol;
      xassert(visibleCursorCol >= 0);

      // 'x' coordinate of the leftmost column of the character cell
      // where the cursor is, i.e., the character that would be deleted
      // if the Delete key were pressed.
      x = leftMargin + fontWidth * visibleCursorCol;

      if (false) {     // thin vertical bar
        paint.setPen(cursorColor);
        paint.drawLine(x,0, x, fontHeight-1);
        paint.drawLine(x-1,0, x-1, fontHeight-1);
      }
      else {           // emacs-like box
        // The character shown inside the box should use the same
        // font as if it were not inside the cursor box, to minimize
        // the visual disruption caused by the cursor's presence.
        //
        // Unfortunately, that leads to some code duplication with the
        // main painting code.
        TextCategory cursorCategory = categories.getCategoryAt(cursorCol);
        FontVariant cursorFV = styleDB->getStyle(cursorCategory).variant;
        bool underlineCursor = false;
        if (cursorFV == FV_UNDERLINE) {
          cursorFV = FV_NORMAL;   // 'cursorFontForFV' does not map FV_UNDERLINE
          underlineCursor = true;
        }
        QtBDFFont *cursorFont = cursorFontForFV[cursorFV];

        paint.setBackground(cursorFont->getBgColor());
        paint.eraseRect(x,0, fontWidth, fontHeight);

        if (line < buffer->numLines() &&
            cursorCol <= buffer->lineLength(line)) {
          // Drawing the block cursor overwrote the character, so we
          // have to draw it again.
          if (line == buffer->numLines() - 1 &&
              cursorCol == buffer->lineLength(line)) {
            // Draw nothing at the end of the last line.
          }
          else {
            this->drawOneChar(paint, cursorFont, QPoint(x, baseline),
                              text[visibleCursorCol]);
          }
        }

        if (underlineCursor) {
          paint.setPen(cursorFont->getFgColor());
          int ulBaseline = baseline + min(UNDERLINE_OFFSET, descent);
          paint.drawLine(x, ulBaseline, x + fontWidth, ulBaseline);
        }
      }

      paint.restore();

    }

    // draw the line buffer to the window
    //needed? //paint.flush();     // server-side pixmap is now complete
    winPaint.drawPixmap(0,y, pixmap);    // draw it
  }

  // at this point the entire window has been painted, so there
  // is no need to "fill the remainder" (there used to be code
  // here that tried to do that)
}


void Editor::drawOneChar(QPainter &paint, QtBDFFont *font, QPoint const &pt, char c)
{
  // My buffer representation uses 'char' without much regard
  // to character encoding.  Here, I'm declaring that this whole
  // time I've been storing some 8-bit encoding consistent with
  // the font I'm using, which is Latin-1.  At some point I need
  // to develop and implement a character encoding strategy.
  int codePoint = (unsigned char)c;

  if (this->visibleWhitespace &&
      (codePoint==' ' || codePoint=='\n')) {
    QRect bounds = font->getNominalCharCell(pt);
    QColor fg = font->getFgColor();
    fg.setAlpha(this->whitespaceOpacity);

    if (codePoint == ' ') {
      // Centered dot.
      paint.fillRect(QRect(bounds.center(), QSize(2,2)), fg);
      return;
    }

    if (codePoint == '\n') {
      // Filled triangle.
      int x1 = bounds.left() + bounds.width() * 1/8;
      int x7 = bounds.left() + bounds.width() * 7/8;
      int y1 = bounds.top() + bounds.height() * 1/8;
      int y7 = bounds.top() + bounds.height() * 7/8;

      paint.setPen(Qt::NoPen);
      paint.setBrush(fg);

      QPoint pts[] = {
        QPoint(x1,y7),
        QPoint(x7,y1),
        QPoint(x7,y7),
      };
      paint.drawConvexPolygon(pts, TABLESIZE(pts));;
      return;
    }
  }

  if (font->hasChar(codePoint)) {
    font->drawChar(paint, pt, codePoint);
  }
  else {
    QRect bounds = font->getNominalCharCell(pt);

    // This is a somewhat expensive thing to do because it requires
    // re-rendering the offscreen glyphs.  Hence, I only do it once
    // I know I need it.
    this->minihexFont->setSameFgBgColors(*font);

    drawHexQuad(*(this->minihexFont), paint, bounds, codePoint);
  }
}


void Editor::setDrawStyle(QPainter &paint,
                          QtBDFFont *&curFont, bool &underlining,
                          StyleDB *db, TextCategory cat)
{
  TextStyle const &ts = db->getStyle(cat);

  // This is needed for underlining since we draw that as a line,
  // whereas otherwise the foreground color comes from the font glyphs.
  paint.setPen(ts.foreground);

  paint.setBackground(ts.background);

  underlining = (ts.variant == FV_UNDERLINE);

  curFont = fontForCategory[cat];
  xassert(curFont);
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
  //redraw();    // 'scrollToCursor' does 'redraw()' automatically
}


void Editor::turnOffSelection()
{
  this->selectEnabled = false;
}

void Editor::turnOnSelection()
{
  if (!this->selectEnabled) {
    this->selectLine = cursorLine();
    this->selectCol = cursorCol();
    this->selectEnabled = true;
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
  if (this->selectEnabled &&
      cursorLine() == this->selectLine &&
      cursorCol() == this->selectCol) {
    turnOffSelection();
  }
}


void printUnhandled(QWidget *parent, char const *msg)
{
  QMessageBox::information(parent, "Oops",
    QString(stringc << "Unhandled exception: " << msg << "\n"
                    << "Save your buffers if you can!"));
}


// unfortunately, this doesn't work if Qt is compiled without
// exception support, as is apparently the case on many (most?)
// linux systems; see e.g.
//   http://lists.trolltech.com/qt-interest/2002-11/msg00048.html
// you therefore have to ensure that exceptions do not propagate into
// Qt stack frames
bool Editor::event(QEvent *e)
{
  try {
    return QWidget::event(e);
  }
  catch (xBase &x) {
    printUnhandled(this, toCStr(x.why()));
    return true;   // clearly it was handled by someone
  }
}


// therefore I'll try this
#define GENERIC_CATCH_BEGIN         \
  try {

#define GENERIC_CATCH_END           \
  }                                 \
  catch (xBase &x) {                \
    printUnhandled(this, toCStr(x.why()));  \
  }


void Editor::keyPressEvent(QKeyEvent *k)
{
  GENERIC_CATCH_BEGIN

  TRACE("input", "keyPress: " << toString(*k));
  HBGrouper hbgrouper(*buffer);

  Qt::KeyboardModifiers modifiers = k->modifiers();

  // We need to map pseudo-keys before the input proxy sees
  // them, because otherwise the proxy may swallow them.
  if (modifiers == Qt::NoModifier) {
    switch (k->key()) {
      case Qt::Key_Escape:
        pseudoKeyPress(IPK_CANCEL);
        return;
    }
  }

  if (modifiers == Qt::ControlModifier) {
    switch (k->key()) {
      case Qt::Key_G:
        pseudoKeyPress(IPK_CANCEL);
        return;
    }
  }

  // Now check with the proxy.
  if (inputProxy && inputProxy->keyPressEvent(k)) {
    return;
  }

  // Ctrl+<key>
  if (modifiers == Qt::ControlModifier) {
    switch (k->key()) {
      case Qt::Key_Insert:
        editCopy();
        break;

      case Qt::Key_PageUp:
        turnOffSelection();
        cursorToTop();
        break;

      case Qt::Key_PageDown:
        turnOffSelection();
        cursorToBottom();
        break;

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        cursorToEndOfNextLine(false);
        break;
      }

      case Qt::Key_W:
        moveView(-1, 0);
        if (cursorLine() > this->lastVisibleLine) {
          cursorUpBy(cursorLine() - this->lastVisibleLine);
        }
        redraw();
        break;

      case Qt::Key_Z:
        moveView(+1, 0);
        if (cursorLine() < this->firstVisibleLine) {
          cursorDownBy(this->firstVisibleLine - cursorLine());
        }
        redraw();
        break;

      case Qt::Key_Up:
        moveViewAndCursor(-1, 0);
        break;

      case Qt::Key_Down:
        moveViewAndCursor(+1, 0);
        break;

      case Qt::Key_Left:
        moveViewAndCursor(0, -1);
        break;

      case Qt::Key_Right:
        moveViewAndCursor(0, +1);
        break;

      case Qt::Key_B:      cursorLeft(false); break;
      case Qt::Key_F:      cursorRight(false); break;
      case Qt::Key_A:      cursorHome(false); break;
      case Qt::Key_E:      cursorEnd(false); break;
      case Qt::Key_P:      cursorUp(false); break;
      case Qt::Key_N:      cursorDown(false); break;
      // emacs' pageup/pagedown are ctrl-v and alt-v, but the
      // latter should be reserved for accessing the menu, so I'm
      // not going to bind either by default

      case Qt::Key_D:
        this->deleteCharAtCursor();
        break;

      case Qt::Key_H:
        this->deleteLeftOfCursor();
        break;

      case Qt::Key_L:
        setView(max(0, cursorLine() - this->visLines()/2), 0);
        scrollToCursor();
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Alt+<key>
  else if (modifiers == Qt::AltModifier) {
    switch (k->key()) {
      case Qt::Key_Left:
        blockIndent(-2);
        break;

      case Qt::Key_Right:
        blockIndent(+2);
        break;

      case Qt::Key_D: {
        // TODO: This does not work on Windows.  The time zone
        // information is wrong.
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        string s = stringf("%d-%02d-%02d %02d:%02d",
                           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                           tm->tm_hour, tm->tm_min);
        insertAtCursor(toCStr(s));
        break;
      }
    }
  }

  // Ctrl+Alt+<key>.  This is where I put commands mainly meant for
  // use while debugging, although Ctrl+Alt+Left/Right (which are
  // handled as menu shortcuts) are ordinary commands.  I recently
  // learned that Ctrl+Alt is used on some keyboards to compose more
  // complex characters, so it is probably best to avoid adding many
  // keybindings for it.
  else if (modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
    switch (k->key()) {
      case Qt::Key_B: {
        breaker();     // breakpoint for debugger
        break;
      }

      case Qt::Key_X: {
        // test exception mechanism...
        THROW(xBase("gratuitous exception"));
        break;
      }

      case Qt::Key_Y: {
        try {
          xbase("another exc");
        }
        catch (xBase &x) {
          QMessageBox::information(this, "got it",
            "got it");
        }
        break;
      }

      case Qt::Key_P: {
        long start = getMilliseconds();
        int frames = 20;
        for (int i=0; i < frames; i++) {
          redraw();
        }
        long elapsed = getMilliseconds() - start;
        QMessageBox::information(this, "perftest",
          qstringb("drew " << frames << " frames in " <<
                   elapsed << " milliseconds, or " <<
                   (elapsed / frames) << " ms/frame"));
        break;
      }

      case Qt::Key_U:
        buffer->core().dumpRepresentation();
        malloc_stats();
        break;

      case Qt::Key_H:
        buffer->printHistory();
        buffer->printHistoryStats();
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Ctrl+Shift+<key>
  else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
    xassert(ctrlShiftDistance > 0);

    switch (k->key()) {
      case Qt::Key_Up:
        moveViewAndCursor(-ctrlShiftDistance, 0);
        break;

      case Qt::Key_Down:
        moveViewAndCursor(+ctrlShiftDistance, 0);
        break;

      case Qt::Key_Left:
        moveViewAndCursor(0, -ctrlShiftDistance);
        break;

      case Qt::Key_Right:
        moveViewAndCursor(0, +ctrlShiftDistance);
        break;

      case Qt::Key_PageUp:
        turnOnSelection();
        cursorToTop();
        break;

      case Qt::Key_PageDown:
        turnOnSelection();
        cursorToBottom();
        break;

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        cursorToEndOfNextLine(true);
        break;
      }

      case Qt::Key_B:      cursorLeft(true); break;
      case Qt::Key_F:      cursorRight(true); break;
      case Qt::Key_A:      cursorHome(true); break;
      case Qt::Key_E:      cursorEnd(true); break;
      case Qt::Key_P:      cursorUp(true); break;
      case Qt::Key_N:      cursorDown(true); break;

      default:
        k->ignore();
        break;
    }
  }

  // <key> and shift-<key>
  else if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier) {
    bool shift = (modifiers == Qt::ShiftModifier);
    switch (k->key()) {
      case Qt::Key_Insert:
        if (shift) {
          editPaste();
        }
        else {
          // TODO: toggle insert/overwrite mode
        }
        break;

      case Qt::Key_Left:     cursorLeft(shift); break;
      case Qt::Key_Right:    cursorRight(shift); break;
      case Qt::Key_Home:     cursorHome(shift); break;
      case Qt::Key_End:      cursorEnd(shift); break;
      case Qt::Key_Up:       cursorUp(shift); break;
      case Qt::Key_Down:     cursorDown(shift); break;
      case Qt::Key_PageUp:   cursorPageUp(shift); break;
      case Qt::Key_PageDown: cursorPageDown(shift); break;

      case Qt::Key_Backspace: {
        if (!shift) {
          this->deleteLeftOfCursor();
        }
        break;
      }

      case Qt::Key_Delete: {
        if (shift) {
          editCut();
        }
        else {
          deleteCharAtCursor();
        }
        break;
      }

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        if (!shift) {
          int lineLength = buffer->lineLength(buffer->line());
          bool hadCharsToRight = (buffer->col() < lineLength);
          bool beyondLineEnd = (buffer->col() > lineLength);
          if (beyondLineEnd) {
            // Move the cursor to the end of the line so
            // that fillToCursor will not add spaces.
            buffer->moveAbsColumn(lineLength);
          }

          // Add newlines if needed so the cursor is on a valid line.
          fillToCursor();

          // typing replaces selection
          if (this->selectEnabled) {
            editDelete();
          }

          buffer->insertNewline();

          // make sure we can see as much to the left as possible
          setFirstVisibleCol(0);

          // auto-indent
          int ind = buffer->getAboveIndentation(cursorLine()-1);
          if (hadCharsToRight) {
            // Insert spaces so the carried forward text starts
            // in the auto-indent column.
            buffer->insertSpaces(ind);
          }
          else {
            // Move the cursor to the auto-indent column but do not
            // fill with spaces.  This way I can press Enter more
            // than once without adding lots of spaces.
            buffer->moveRelCursor(0, ind);
          }

          scrollToCursor();
        }
        break;
      }

      case Qt::Key_Tab: {
        if (shift) {
          // In my testing on Windows, this does not get executed,
          // rather the key is delivered as Key_Backtab.  I do not
          // know if the same is true on Linux and Mac, so I will
          // leave this here just in case.
          blockIndent(-2);
        }
        else {
          // TODO: This should insert a Tab character if nothing is
          // selected.  But right now it does not, and if the file
          // already has a Tab in it, it is rendered the same as a
          // space character.
          blockIndent(+2);
        }
        break;
      }

      case Qt::Key_Backtab: {
        blockIndent(-2);
        break;
      }

      default: {
        QString text = k->text();
        if (text.length() && text[0].isPrint()) {
          fillToCursor();
          //buffer->changed = true;

          // typing replaces selection
          if (this->selectEnabled) {
            editDelete();
          }
          // insert this character at the cursor
          QByteArray utf8(text.toUtf8());
          buffer->insertLR(false /*left*/, utf8.constData(), utf8.length());
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

  GENERIC_CATCH_END
}


void Editor::keyReleaseEvent(QKeyEvent *k)
{
  TRACE("input", "keyRelease: " << toString(*k));

  k->ignore();
}


void Editor::insertAtCursor(char const *text)
{
  //buffer->changed = true;
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
  //buffer->changed = true;
  scrollToCursor();
}


void Editor::deleteLeftOfCursor()
{
  fillToCursor();
  //buffer->changed = true;

  if (this->selectEnabled) {
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


// for a particular dimension, return the new start coordinate
// of the viewport
int Editor::stcHelper(int firstVis, int lastVis, int cur, int gap)
{
  bool center = false;
  if (gap == -1) {
    center = true;
    gap = 0;
  }

  int width = lastVis - firstVis + 1;

  bool changed = false;
  if (cur-gap < firstVis) {
    firstVis = max(0, cur-gap);
    changed = true;
  }
  else if (cur+gap > lastVis) {
    firstVis += cur+gap - lastVis;
    changed = true;
  }

  if (changed && center) {
    // we had to adjust the viewport; make it actually centered
    firstVis = max(0, cur - width/2);
  }

  return firstVis;
}

void Editor::scrollToCursor_noRedraw(int edgeGap)
{
  int fvline = stcHelper(this->firstVisibleLine,
                         this->lastVisibleLine,
                         cursorLine(), edgeGap);

  int fvcol = stcHelper(this->firstVisibleCol,
                        this->lastVisibleCol,
                        cursorCol(), edgeGap);

  setView(fvline, fvcol);
}

void Editor::scrollToCursor(int edgeGap)
{
  scrollToCursor_noRedraw(edgeGap);
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
  scrollToCursor_noRedraw();

  // move viewport, but remember original so we can tell
  // when there's truncation
  int origVL = this->firstVisibleLine;
  int origVC = this->firstVisibleCol;
  moveView(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  moveCursorBy(this->firstVisibleLine - origVL,
               this->firstVisibleCol - origVC);

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

  int newLine = y/lineHeight() + this->firstVisibleLine;
  int newCol = x/fontWidth + this->firstVisibleCol;

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
void Editor::editUndo()
{
  if (buffer->canUndo()) {
    buffer->undo();
    turnOffSelection();
    scrollToCursor();
  }
  else {
    QMessageBox::information(this, "Can't undo",
      "There are no actions to undo in the history.");
  }
}

void Editor::editRedo()
{
  if (buffer->canRedo()) {
    buffer->redo();
    turnOffSelection();
    scrollToCursor();
  }
  else {
    QMessageBox::information(this, "Can't redo",
      "There are no actions to redo in the history.");
  }
}


void Editor::editCut()
{
  if (this->selectEnabled) {
    editCopy();
    this->selectEnabled = true;    // counteract something editCopy() does
    editDelete();
  }
}


void Editor::editCopy()
{
  if (this->selectEnabled) {
    // get selected text
    string sel = getSelectedText();

    // put it into the clipboard
    QClipboard *cb = QApplication::clipboard();
    cb->setText(toQString(sel));

    // un-highlight the selection, which is what emacs does and
    // I'm now used to
    this->selectEnabled = false;
    redraw();
  }
}


void Editor::editPaste()
{
  // get contents of clipboard
  QClipboard *cb = QApplication::clipboard();
  QString text = cb->text();
  if (text.isEmpty()) {
    QMessageBox::information(this, "Info", "The clipboard is empty.");
  }
  else {
    fillToCursor();

    // remove what's selected, if anything
    editDelete();

    // insert at cursor
    QByteArray utf8(text.toUtf8());
    insertAtCursor(utf8.constData());
  }
}


void Editor::editDelete()
{
  if (this->selectEnabled) {
    normalizeSelect();
    //buffer->changed = true;
    buffer->deleteTextRange(selLowLine, selLowCol, selHighLine, selHighCol);

    this->selectEnabled = false;
    scrollToCursor();
  }
}


void Editor::showInfo(char const *infoString)
{
  QWidget *main = this->window();

  if (!this->infoBox) {
    infoBox = new QLabel(main);
    infoBox->setObjectName("infoBox");
    infoBox->setForegroundRole(QPalette::ToolTipText);
    infoBox->setBackgroundRole(QPalette::ToolTipBase);
    infoBox->setAutoFillBackground(true);
    infoBox->setIndent(2);
  }

  infoBox->setText(infoString);

  // compute a good size for the label
  QFontMetrics fm(infoBox->font());
  QSize sz = fm.size(0, infoString);
  infoBox->resize(sz.width() + 4, sz.height() + 2);

  // Compute a position just below the lower-left corner
  // of the cursor box, in the coordinates of 'this'.
  QPoint target(
    (cursorCol() - this->firstVisibleCol) * fontWidth,
    (cursorLine() - this->firstVisibleLine + 1) * fontHeight + 1);

  // Translate that to the coordinates of 'main'.
  target = this->mapTo(main, target);
  infoBox->move(target);

  // If the box goes beyond the right edge of the window, pull it back
  // to the left to keep it inside.
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
  moveViewAndCursor(- this->visLines(), 0);
}

void Editor::cursorPageDown(bool shift)
{
  turnSelection(shift);
  moveViewAndCursor(+ this->visLines(), 0);
}


void Editor::cursorToEndOfNextLine(bool shift)
{
  turnSelection(shift);
  int line = buffer->line();
  this->buffer->moveAbsCursor(line+1, buffer->lineLengthLoose(line+1));
  scrollToCursor();
}


void Editor::deleteCharAtCursor()
{
  fillToCursor();

  if (this->selectEnabled) {
    editDelete();
  }
  else {
    if (this->buffer->cursorAtEnd()) {
      // Nothing to do since no characters are to the right.
    }
    else if (cursorCol() == buffer->lineLength(cursorLine())) {
      // splice next line onto this one
      spliceNextLine();
    }
    else /* cursor < lineLength */ {
      // delete character to right of cursor
      buffer->deleteText(1);
    }
  }

  scrollToCursor();
}


void Editor::blockIndent(int amt)
{
  if (!this->selectEnabled) {
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
  if (!this->selectEnabled) {
    return "";
  }
  else {
    normalizeSelect();   // this is why this method is not 'const' ...
    return buffer->getTextRange(selLowLine, selLowCol, selHighLine, selHighCol);
  }
}


// ----------------- nonfocus situation ------------------
void Editor::focusInEvent(QFocusEvent *e)
{
  TRACE("focus", "editor(" << (void*)this << "): focus in");
  QWidget::focusInEvent(e);

  // move the editing cursor to where I last had it
  cursorTo(nonfocusCursorLine, nonfocusCursorCol);

  // I don't want to listen while I'm adding changes of
  // my own, because the way the view moves (etc.) on
  // changes is different
  stopListening();
}

void Editor::focusOutEvent(QFocusEvent *e)
{
  TRACE("focus", "editor(" << (void*)this << "): focus out");
  QWidget::focusOutEvent(e);

  stopListening();    // just in case

  // listen to my buffer for any changes coming from
  // other windows
  startListening();
}


void Editor::stopListening()
{
  if (listening) {
    // remove myself from the list
    buffer->core().observers.removeItem(this);

    listening = false;
  }
}

void Editor::startListening()
{
  xassert(!listening);

  // add myself to the list
  buffer->core().observers.append(this);
  listening = true;

  // remember the buffer's current cursor position
  nonfocusCursorLine = buffer->line();
  nonfocusCursorCol = buffer->col();
}


// General goal for dealing with inserted lines:  The cursor in the
// nonfocus window should not change its vertical location within the
// window (# of pixels from top window edge), and should remain on the
// same line (sequence of chars).

void Editor::observeInsertLine(BufferCore const &buf, int line)
{
  if (line <= nonfocusCursorLine) {
    nonfocusCursorLine++;
    moveView(+1, 0);
  }

  redraw();
}

void Editor::observeDeleteLine(BufferCore const &buf, int line)
{
  if (line < nonfocusCursorLine) {
    nonfocusCursorLine--;
    moveView(-1, 0);
  }

  redraw();
}


// For inserted characters, I don't do anything special, so
// the cursor says in the same column of text.

void Editor::observeInsertText(BufferCore const &buf, int line, int col, char const *text, int length)
{
  redraw();
}

void Editor::observeDeleteText(BufferCore const &buf, int line, int col, int length)
{
  redraw();
}


void Editor::inputProxyDetaching()
{
  TRACE("mode", "clearing mode pixmap");
  QPixmap nullPixmap;
  this->status->mode->setPixmap(nullPixmap);
}


void Editor::pseudoKeyPress(InputPseudoKey pkey)
{
  if (inputProxy && inputProxy->pseudoKeyPress(pkey)) {
    // handled
    return;
  }

  // Handle myself.
  switch (pkey) {
    default:
      xfailure("invalid pseudo key");

    case IPK_CANCEL:
      // do nothing; in other modes this will cancel out

      // well, almost nothing
      this->hitText = "";
      redraw();
      break;
  }
}


// EOF
