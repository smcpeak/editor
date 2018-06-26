// editor-widget.cc
// code for editor-widget.h

#include "editor-widget.h"   // this module

// this dir
#include "inputproxy.h"      // InputProxy
#include "justify.h"         // justifyNearLine
#include "main.h"            // GlobalState
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

// Number of columns to move for Ctrl+Shift+<arrow>.
int const CTRL_SHIFT_DISTANCE = 10;


// ---------------------- EditorWidget --------------------------------
int EditorWidget::objectCount = 0;

EditorWidget::EditorWidget(TextDocumentFile *tdf, StatusDisplay *status_,
                           QWidget *parent)
  : QWidget(parent),
    infoBox(NULL),
    status(status_),
    editor(new TextDocumentFileEditor(tdf)),
    topMargin(1),
    leftMargin(1),
    interLineSpace(0),
    cursorColor(0xFF, 0xFF, 0xFF),       // white
    fontForCategory(NUM_STANDARD_TEXT_CATEGORIES),
    cursorFontForFV(FV_BOLD + 1),
    minihexFont(),
    visibleWhitespace(true),
    whitespaceOpacity(32),
    softMarginColumn(72),
    visibleSoftMargin(true),
    softMarginColor(0xFF, 0xFF, 0xFF, 32),
    inputProxy(NULL),
    // font metrics inited by setFont()
    listening(false),
    nonfocusCursorLine(0), nonfocusCursorCol(0),
    ignoreScrollSignals(false)
{
  xassert(status_);

  setFonts(bdfFontData_editor14r,
           bdfFontData_editor14i,
           bdfFontData_editor14b);

  setCursor(Qt::IBeamCursor);

  // required to accept focus
  setFocusPolicy(Qt::StrongFocus);

  resetView();

  EditorWidget::objectCount++;
}

EditorWidget::~EditorWidget()
{
  EditorWidget::objectCount--;

  if (inputProxy) {
    inputProxy->detach();
  }

  this->stopListening();

  // Do this explicitly just for clarity.
  this->editor = NULL;
  m_editors.deleteAll();

  fontForCategory.deleteAll();
}


void EditorWidget::selfCheck() const
{
  // Check that 'editor' is among 'm_editors' and that the files in
  // 'm_editors' are a subset of GlobalState::documentFiles.
  bool foundEditor = false;
  FOREACH_OBJLIST(TextDocumentFileEditor, m_editors, iter) {
    TextDocumentFileEditor const *tdfe = iter.data();
    if (editor == tdfe) {
      foundEditor = true;
    }
    tdfe->selfCheck();
    xassert(GlobalState::global_globalState->documentFiles.contains(tdfe->file));
  }
  xassert(foundEditor);

  // There should never be more m_editors than documentFiles.
  xassert(GlobalState::global_globalState->documentFiles.count() >= m_editors.count());
}


void EditorWidget::cursorTo(int line, int col)
{
  editor->moveAbsCursor(line, col);

  // set the nonfocus location too, in case the we happen to
  // not have the focus right now (e.g. the Alt+G dialog);
  // actually, the need for this exposes the fact that my current
  // solution to nonfocus cursor issues isn't very good.. hmm...
  nonfocusCursorLine = line;
  nonfocusCursorCol = col;
}

void EditorWidget::resetView()
{
  if (editor) {
    cursorTo(0, 0);
    editor->clearMark();
  }
  setView(TextCoord(0,0));
}


void EditorWidget::getSelectRegionForCursor(TextCoord cursor,
  TextCoord &selLow, TextCoord &selHigh) const
{
  if (!this->selectEnabled()) {
    selLow = selHigh = cursor;
  }
  else {
    TextCoord const m = this->mark();
    if (cursor <= m) {
      selLow = cursor;
      selHigh = m;
    }
    else {
      selLow = m;
      selHigh = cursor;
    }
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


void EditorWidget::setFonts(char const *normal, char const *italic, char const *bold)
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


void EditorWidget::setDocumentFile(TextDocumentFile *file)
{
  bool wasListening = this->listening;
  if (wasListening) {
    this->stopListening();
  }

  this->editor = this->getOrMakeEditor(file);

  if (wasListening) {
    this->startListening();
  }

  this->redraw();
}


EditorWidget::TextDocumentFileEditor *
  EditorWidget::getOrMakeEditor(TextDocumentFile *file)
{
  // Look for an existing editor for this file.
  FOREACH_OBJLIST_NC(TextDocumentFileEditor, m_editors, iter) {
    if (iter.data()->file == file) {
      return iter.data();
    }
  }

  // Have to make a new one.
  TextDocumentFileEditor *ret = new TextDocumentFileEditor(file);
  m_editors.prepend(ret);
  return ret;
}


void EditorWidget::forgetAboutFile(TextDocumentFile *file)
{
  // Remove 'file' from my list.
  for(ObjListMutator< TextDocumentFileEditor > mut(m_editors); !mut.isDone(); ) {
    if (mut.data()->file == file) {
      mut.remove();
    }
    else {
      mut.adv();
    }
  }

  // Change files if that was the one we were editing.
  if (this->editor->file == file) {
    // This dependence on GlobalState is questionable...
    this->setDocumentFile(
      GlobalState::global_globalState->documentFiles.first());
  }
}


TextDocumentFile *EditorWidget::getDocumentFile() const
{
  xassert(this->editor);
  xassert(this->editor->file);
  return this->editor->file;
}


TextDocumentEditor *EditorWidget::getDocumentEditor()
{
  xassert(this->editor);
  return this->editor;
}


void EditorWidget::redraw()
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


void EditorWidget::selectCursorLine()
{
  editor->selectCursorLine();
}


void EditorWidget::setView(TextCoord newFirstVisible)
{
  xassert(newFirstVisible.nonNegative());

  if (newFirstVisible != editor->firstVisible()) {
    editor->setFirstVisible(newFirstVisible);

    updateView();

    TRACE("setView", "new firstVis is " << firstVisStr());
  }
}


void EditorWidget::moveView(int deltaLine, int deltaCol)
{
  int line = max(0, editor->firstVisible().line + deltaLine);
  int col = max(0, editor->firstVisible().column + deltaCol);

  this->setView(TextCoord(line, col));
}


void EditorWidget::updateView()
{
  int h = this->height();
  int w = this->width();

  if (this->fontHeight && this->fontWidth) {
    // calculate viewport size
    editor->setVisibleSize(
      (h - this->topMargin) / this->lineHeight(),
      (w - this->leftMargin) / this->fontWidth);
  }
  else {
    // font info not set, leave them alone
  }
}


void EditorWidget::resizeEvent(QResizeEvent *r)
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
void EditorWidget::paintEvent(QPaintEvent *ev)
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

void EditorWidget::updateFrame(QPaintEvent *ev, int cursorLine, int cursorCol)
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
  int const firstCol = this->firstVisibleCol();
  int const firstLine = this->firstVisibleLine();

  // I think it might be useful to support negative values for these
  // variables, but the code below is not prepared to deal with such
  // values at this time
  xassert(firstLine >= 0);
  xassert(firstCol >= 0);

  // another santiy check
  xassert(lineHeight() > 0);

  // Buffer that will be used for each visible line of text.
  Array<char> text(visibleCols);

  // Get region of selected text.
  TextCoord selLow, selHigh;
  this->getSelectRegionForCursor(TextCoord(cursorLine, cursorCol), selLow, selHigh);

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

    // fill with text from the file
    if (line < editor->numLines()) {
      // 1 if we will behave as though a newline character is
      // at the end of this line.
      int newlineAdjust = 0;
      if (this->visibleWhitespace && line < editor->numLines()-1) {
        newlineAdjust = 1;
      }

      // Line length including possible synthesized newline.
      int const lineLen = editor->lineLength(line) + newlineAdjust;

      if (firstCol < lineLen) {
        // First get the text without any extra newline.
        int const amt = min(lineLen-newlineAdjust - firstCol, visibleCols);
        editor->getLine(TextCoord(line, firstCol), text, amt);
        visibleLineChars = amt;

        // Now possibly add the newline.
        if (visibleLineChars < visibleCols && newlineAdjust != 0) {
          text[visibleLineChars++] = '\n';
        }
      }

      // apply highlighting
      if (editor->file->highlighter) {
        editor->file->highlighter->
          highlight(editor->core(), line, categories);
      }

      // Show search hits.
      if (this->hitText.length() > 0) {
        TextCoord hitTC(line, 0);
        TextDocumentEditor::FindStringFlags const hitTextFlags =
          this->hitTextFlags | TextDocumentEditor::FS_ONE_LINE;

        while (editor->findString(hitTC /*INOUT*/,
                                  toCStr(this->hitText),
                                  hitTextFlags)) {
          categories.overlay(hitTC.column, this->hitText.length(), TC_HITS);
          hitTC.column++;
        }
      }
    }
    xassert(visibleLineChars <= visibleCols);

    // incorporate effect of selection
    if (this->selectEnabled() &&
        selLow.line <= line && line <= selHigh.line)
    {
      if (selLow.line < line && line < selHigh.line) {
        // entire line is selected
        categories.overlay(0, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selLow.line < line && line == selHigh.line) {
        // first half of line is selected
        if (selHigh.column) {
          categories.overlay(0, selHigh.column, TC_SELECTION);
        }
      }
      else if (selLow.line == line && line < selHigh.line) {
        // right half of line is selected
        categories.overlay(selLow.column, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selLow.line == line && line == selHigh.line) {
        // middle part of line is selected
        if (selHigh.column != selLow.column) {
          categories.overlay(selLow.column, selHigh.column-selLow.column, TC_SELECTION);
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

        if (line < editor->numLines() &&
            cursorCol <= editor->lineLength(line)) {
          // Drawing the block cursor overwrote the character, so we
          // have to draw it again.
          if (line == editor->numLines() - 1 &&
              cursorCol == editor->lineLength(line)) {
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

    // Draw a soft margin indicator.
    if (this->visibleSoftMargin) {
      paint.save();
      paint.setPen(this->softMarginColor);

      int x = leftMargin + fontWidth * (this->softMarginColumn - firstCol);
      paint.drawLine(x, 0, x, fontHeight-1);

      paint.restore();
    }

    // draw the line buffer to the window
    winPaint.drawPixmap(0,y, pixmap);    // draw it
  }
}


void EditorWidget::drawOneChar(QPainter &paint, QtBDFFont *font, QPoint const &pt, char c)
{
  // My document representation uses 'char' without much regard
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


void EditorWidget::setDrawStyle(QPainter &paint,
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


void EditorWidget::cursorToTop()
{
  cursorTo(0, 0);
  scrollToCursor();
}

void EditorWidget::cursorToBottom()
{
  cursorTo(max(editor->numLines()-1,0), 0);
  scrollToCursor();
  //redraw();    // 'scrollToCursor' does 'redraw()' automatically
}


void EditorWidget::turnOffSelection()
{
  this->clearMark();
}

void EditorWidget::turnOnSelection()
{
  if (!this->selectEnabled()) {
    this->setMark(this->textCursor());
  }
}

void EditorWidget::turnSelection(bool on)
{
  if (on) {
    turnOnSelection();
  }
  else {
    turnOffSelection();
  }
}

void EditorWidget::clearSelIfEmpty()
{
  if (this->selectEnabled() &&
      this->textCursor() == this->mark()) {
    turnOffSelection();
  }
}


void printUnhandled(QWidget *parent, char const *msg)
{
  QMessageBox::information(parent, "Oops",
    QString(stringc << "Unhandled exception: " << msg << "\n"
                    << "Save your files if you can!"));
}


// unfortunately, this doesn't work if Qt is compiled without
// exception support, as is apparently the case on many (most?)
// linux systems; see e.g.
//   http://lists.trolltech.com/qt-interest/2002-11/msg00048.html
// you therefore have to ensure that exceptions do not propagate into
// Qt stack frames
bool EditorWidget::event(QEvent *e)
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


void EditorWidget::keyPressEvent(QKeyEvent *k)
{
  GENERIC_CATCH_BEGIN

  TRACE("input", "keyPress: " << toString(*k));
  UndoHistoryGrouper hbgrouper(*editor);

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
        if (cursorLine() > this->lastVisibleLine()) {
          cursorUpBy(cursorLine() - this->lastVisibleLine());
        }
        redraw();
        break;

      case Qt::Key_Z:
        moveView(+1, 0);
        if (cursorLine() < this->firstVisibleLine()) {
          cursorDownBy(this->firstVisibleLine() - cursorLine());
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
        setView(TextCoord(max(0, cursorLine() - this->visLines()/2), 0));
        scrollToCursor();
        break;

      case Qt::Key_J:
        if (!editSafetyCheck()) {
          return;
        }
        if (selectEnabled()) {
          QMessageBox::information(this, "Unimp", "unimplemented");
        }
        else {
          this->justifyNearCursorLine();
        }
        break;

      case Qt::Key_K:
        if (!editSafetyCheck()) {
          return;
        }
        if (!selectEnabled()) {
          selectCursorLine();
        }
        editCut();
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
        editor->core().dumpRepresentation();
        malloc_stats();
        break;

      case Qt::Key_H:
        editor->doc()->printHistory();
        editor->doc()->printHistoryStats();
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Ctrl+Shift+<key>
  else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
    switch (k->key()) {
      case Qt::Key_Up:
        moveViewAndCursor(-CTRL_SHIFT_DISTANCE, 0);
        break;

      case Qt::Key_Down:
        moveViewAndCursor(+CTRL_SHIFT_DISTANCE, 0);
        break;

      case Qt::Key_Left:
        moveViewAndCursor(0, -CTRL_SHIFT_DISTANCE);
        break;

      case Qt::Key_Right:
        moveViewAndCursor(0, +CTRL_SHIFT_DISTANCE);
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
        if (!editSafetyCheck()) {
          return;
        }
        if (!shift) {
          this->deleteLeftOfCursor();
        }
        break;
      }

      case Qt::Key_Delete: {
        if (!editSafetyCheck()) {
          return;
        }
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
          if (!editSafetyCheck()) {
            return;
          }

          int lineLength = editor->lineLength(editor->line());
          bool hadCharsToRight = (editor->col() < lineLength);
          bool beyondLineEnd = (editor->col() > lineLength);
          if (beyondLineEnd) {
            // Move the cursor to the end of the line so
            // that fillToCursor will not add spaces.
            editor->moveAbsColumn(lineLength);
          }

          // Add newlines if needed so the cursor is on a valid line.
          fillToCursor();

          // typing replaces selection
          if (this->selectEnabled()) {
            editDelete();
          }

          editor->insertNewline();

          // make sure we can see as much to the left as possible
          setFirstVisibleCol(0);

          // auto-indent
          int ind = editor->getAboveIndentation(cursorLine()-1);
          if (hadCharsToRight) {
            // Insert spaces so the carried forward text starts
            // in the auto-indent column.
            editor->insertSpaces(ind);
          }
          else {
            // Move the cursor to the auto-indent column but do not
            // fill with spaces.  This way I can press Enter more
            // than once without adding lots of spaces.
            editor->moveRelCursor(0, ind);
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
          if (!editSafetyCheck()) {
            return;
          }
          fillToCursor();
          //docFile->changed = true;

          // typing replaces selection
          if (this->selectEnabled()) {
            editDelete();
          }
          // insert this character at the cursor
          QByteArray utf8(text.toUtf8());
          editor->insertLR(false /*left*/, utf8.constData(), utf8.length());
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


void EditorWidget::keyReleaseEvent(QKeyEvent *k)
{
  TRACE("input", "keyRelease: " << toString(*k));

  // Not sure if this is the best place for this, but it seems
  // worth a try.
  this->selfCheck();

  k->ignore();
}


void EditorWidget::insertAtCursor(char const *text)
{
  //docFile->changed = true;
  editor->insertText(text);
  scrollToCursor();
}

void EditorWidget::deleteAtCursor(int amt)
{
  xassert(amt >= 0);
  if (amt == 0) {
    return;
  }

  fillToCursor();
  editor->deleteLR(true /*left*/, amt);
  //docFile->changed = true;
  scrollToCursor();
}


void EditorWidget::deleteLeftOfCursor()
{
  if (this->selectEnabled()) {
    editDelete();
  }
  else if (cursorCol() == 0) {
    if (cursorLine() == 0) {
      // do nothing
    }
    else if (cursorLine() > editor->numLines()-1) {
      // Move cursor up non-destructively.
      cursorUp(false /*shift*/);
    }
    else {
      // move to end of previous line
      editor->moveToPrevLineEnd();

      // splice them together
      spliceNextLine();
    }
  }
  else if (cursorCol() > editor->lineLengthLoose(cursorLine())) {
    // Move cursor left non-destructively.
    cursorLeft(false /*shift*/);
  }
  else {
    // remove the character to the left of the cursor
    editor->deleteLR(true /*left*/, 1);
  }

  scrollToCursor();
}


void EditorWidget::fillToCursor()
{
  editor->fillToCursor();
}


void EditorWidget::spliceNextLine()
{
  // cursor must be at the end of a line
  xassert(cursorCol() == editor->lineLength(cursorLine()));

  editor->deleteChar();
}


void EditorWidget::justifyNearCursorLine()
{
  justifyNearLine(*editor, this->cursorLine(), this->softMarginColumn);
  this->scrollToCursor();
}


// for a particular dimension, return the new start coordinate
// of the viewport
int EditorWidget::stcHelper(int firstVis, int lastVis, int cur, int gap)
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

void EditorWidget::scrollToCursor_noRedraw(int edgeGap)
{
  int fvline = stcHelper(this->firstVisibleLine(),
                         this->lastVisibleLine(),
                         cursorLine(), edgeGap);

  int fvcol = stcHelper(this->firstVisibleCol(),
                        this->lastVisibleCol(),
                        cursorCol(), edgeGap);

  setView(TextCoord(fvline, fvcol));
}

void EditorWidget::scrollToCursor(int edgeGap)
{
  scrollToCursor_noRedraw(edgeGap);
  redraw();
}


STATICDEF string EditorWidget::lineColStr(TextCoord tc)
{
  return stringc << tc;
}

void EditorWidget::moveViewAndCursor(int deltaLine, int deltaCol)
{
  TRACE("moveViewAndCursor",
        "start: firstVis=" << firstVisStr()
     << ", cursor=" << cursorStr()
     << ", delta=" << lineColStr(TextCoord(deltaLine, deltaCol)));

  // first make sure the view contains the cursor
  scrollToCursor_noRedraw();

  // move viewport, but remember original so we can tell
  // when there's truncation
  int origVL = this->firstVisibleLine();
  int origVC = this->firstVisibleCol();
  moveView(deltaLine, deltaCol);

  // now move cursor by the amount that the viewport moved
  moveCursorBy(this->firstVisibleLine() - origVL,
               this->firstVisibleCol() - origVC);

  TRACE("moveViewAndCursor",
        "end: firstVis=" << firstVisStr() <<
        ", cursor=" << cursorStr());

  // redraw display
  redraw();
}


void EditorWidget::scrollToLine(int line)
{
  if (!ignoreScrollSignals) {
    xassert(line >= 0);
    setFirstVisibleLine(line);
    redraw();
  }
}

void EditorWidget::scrollToCol(int col)
{
  if (!ignoreScrollSignals) {
    xassert(col >= 0);
    setFirstVisibleCol(col);
    redraw();
  }
}


void EditorWidget::setCursorToClickLoc(QMouseEvent *m)
{
  int x = m->x();
  int y = m->y();

  // subtract off the margin, but don't let either coord go negative
  inc(x, -leftMargin);
  inc(y, -topMargin);

  int newLine = y/lineHeight() + this->firstVisibleLine();
  int newCol = x/fontWidth + this->firstVisibleCol();

  //printf("click: (%d,%d)     goto line %d, col %d\n",
  //       x, y, newLine, newCol);

  cursorTo(newLine, newCol);

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
}


void EditorWidget::mousePressEvent(QMouseEvent *m)
{
  // get rid of popups?
  QWidget::mousePressEvent(m);

  turnOffSelection();
  setCursorToClickLoc(m);

  redraw();
}


void EditorWidget::mouseMoveEvent(QMouseEvent *m)
{
  QWidget::mouseMoveEvent(m);

  turnOnSelection();
  setCursorToClickLoc(m);
  clearSelIfEmpty();

  redraw();
}


void EditorWidget::mouseReleaseEvent(QMouseEvent *m)
{
  QWidget::mouseReleaseEvent(m);

  turnOnSelection();
  setCursorToClickLoc(m);
  clearSelIfEmpty();

  redraw();
}


// ----------------------- edit menu -----------------------
void EditorWidget::editUndo()
{
  if (editor->canUndo()) {
    editor->undo();
    turnOffSelection();
    scrollToCursor();
  }
  else {
    QMessageBox::information(this, "Can't undo",
      "There are no actions to undo in the history.");
  }
}

void EditorWidget::editRedo()
{
  if (editor->canRedo()) {
    editor->redo();
    turnOffSelection();
    scrollToCursor();
  }
  else {
    QMessageBox::information(this, "Can't redo",
      "There are no actions to redo in the history.");
  }
}


void EditorWidget::editCut()
{
  if (this->selectEnabled()) {
    if (!editSafetyCheck()) {
      return;
    }

    this->innerEditCopy(false /*clearSelection*/);
    this->editDelete();
  }
}


void EditorWidget::editCopy()
{
  // un-highlight the selection, which is what emacs does and
  // I'm now used to
  this->innerEditCopy(true /*clearSelection*/);
  redraw();
}

void EditorWidget::innerEditCopy(bool clearSelection)
{
  if (this->selectEnabled()) {
    // get selected text
    string sel = getSelectedText();

    // put it into the clipboard
    QClipboard *cb = QApplication::clipboard();
    cb->setText(toQString(sel));

    if (clearSelection) {
      this->clearMark();
    }
  }
}


void EditorWidget::editPaste()
{
  if (!editSafetyCheck()) {
    return;
  }

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


void EditorWidget::editDelete()
{
  if (this->selectEnabled()) {
    if (!editSafetyCheck()) {
      return;
    }

    TextCoord selLow, selHigh;
    this->getSelectRegion(selLow, selHigh);
    editor->deleteTextRange(selLow.line, selLow.column,
                            selHigh.line, selHigh.column);

    this->clearMark();
    scrollToCursor();
  }
}


void EditorWidget::showInfo(char const *infoString)
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
    (cursorCol() - this->firstVisibleCol()) * fontWidth,
    (cursorLine() - this->firstVisibleLine() + 1) * fontHeight + 1);

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

void EditorWidget::hideInfo()
{
  if (infoBox) {
    delete infoBox;
    infoBox = NULL;
  }
}


void EditorWidget::cursorLeft(bool shift)
{
  turnSelection(shift);
  cursorLeftBy(1);
  scrollToCursor();
}

void EditorWidget::cursorRight(bool shift)
{
  turnSelection(shift);
  cursorRightBy(1);
  scrollToCursor();
}

void EditorWidget::cursorHome(bool shift)
{
  turnSelection(shift);
  editor->moveAbsColumn(0);
  scrollToCursor();
}

void EditorWidget::cursorEnd(bool shift)
{
  turnSelection(shift);
  editor->moveAbsColumn(editor->lineLength(cursorLine()));
  scrollToCursor();
}

void EditorWidget::cursorUp(bool shift)
{
  turnSelection(shift);
  cursorUpBy(1);
  scrollToCursor();
}

void EditorWidget::cursorDown(bool shift)
{
  // allows cursor past EOF..
  turnSelection(shift);
  cursorDownBy(1);
  scrollToCursor();
}

void EditorWidget::cursorPageUp(bool shift)
{
  turnSelection(shift);
  moveViewAndCursor(- this->visLines(), 0);
}

void EditorWidget::cursorPageDown(bool shift)
{
  turnSelection(shift);
  moveViewAndCursor(+ this->visLines(), 0);
}


void EditorWidget::cursorToEndOfNextLine(bool shift)
{
  turnSelection(shift);
  int line = editor->line();
  this->editor->moveAbsCursor(line+1, editor->lineLengthLoose(line+1));
  scrollToCursor();
}


void EditorWidget::deleteCharAtCursor()
{
  fillToCursor();

  if (this->selectEnabled()) {
    editDelete();
  }
  else {
    if (this->editor->cursorAtEnd()) {
      // Nothing to do since no characters are to the right.
    }
    else if (cursorCol() == editor->lineLength(cursorLine())) {
      // splice next line onto this one
      spliceNextLine();
    }
    else /* cursor < lineLength */ {
      // delete character to right of cursor
      editor->deleteText(1);
    }
  }

  scrollToCursor();
}


void EditorWidget::blockIndent(int amt)
{
  if (!this->selectEnabled()) {
    return;      // nop
  }

  TextCoord selLow, selHigh;
  this->getSelectRegion(selLow, selHigh);

  int endLine = (selHigh.column==0? selHigh.line-1 : selHigh.line);
  endLine = min(endLine, editor->numLines()-1);
  editor->indentLines(selLow.line, endLine-selLow.line+1, amt);

  redraw();
}


string EditorWidget::getSelectedText() const
{
  if (!this->selectEnabled()) {
    return "";
  }
  else {
    TextCoord selLow, selHigh;
    this->getSelectRegion(selLow, selHigh);
    return editor->getTextRange(selLow, selHigh);
  }
}


// ----------------- nonfocus situation ------------------
void EditorWidget::focusInEvent(QFocusEvent *e)
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

void EditorWidget::focusOutEvent(QFocusEvent *e)
{
  TRACE("focus", "editor(" << (void*)this << "): focus out");
  QWidget::focusOutEvent(e);

  stopListening();    // just in case

  // listen to my docFile for any changes coming from
  // other windows
  startListening();
}


void EditorWidget::stopListening()
{
  if (listening) {
    // remove myself from the list
    editor->core().observers.removeItem(this);

    listening = false;
  }
}

void EditorWidget::startListening()
{
  xassert(!listening);

  // add myself to the list
  editor->core().observers.append(this);
  listening = true;

  // remember the docFile's current cursor position
  nonfocusCursorLine = editor->line();
  nonfocusCursorCol = editor->col();
}


// General goal for dealing with inserted lines:  The cursor in the
// nonfocus window should not change its vertical location within the
// window (# of pixels from top window edge), and should remain on the
// same line (sequence of chars).

void EditorWidget::observeInsertLine(TextDocumentCore const &buf, int line)
{
  if (line <= nonfocusCursorLine) {
    nonfocusCursorLine++;
    moveView(+1, 0);
  }

  redraw();
}

void EditorWidget::observeDeleteLine(TextDocumentCore const &buf, int line)
{
  if (line < nonfocusCursorLine) {
    nonfocusCursorLine--;
    moveView(-1, 0);
  }

  redraw();
}


// For inserted characters, I don't do anything special, so
// the cursor says in the same column of text.

void EditorWidget::observeInsertText(TextDocumentCore const &, TextCoord, char const *, int)
{
  redraw();
}

void EditorWidget::observeDeleteText(TextDocumentCore const &, TextCoord, int)
{
  redraw();
}


void EditorWidget::inputProxyDetaching()
{
  TRACE("mode", "clearing mode pixmap");
  QPixmap nullPixmap;
  this->status->mode->setPixmap(nullPixmap);
}


void EditorWidget::pseudoKeyPress(InputPseudoKey pkey)
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


// This is not called before every possible editing operation, since
// that would add considerable clutter, just the most common.  A
// missing edit safety check merely delays the user getting the
// warning until they try to save.
//
// There is a danger in scattering the warnings too widely because I
// might end up prompting in an inappropriate context, so currently
// the warnings are only in places where I can clearly see that the
// user just initiated an edit action and it is therefore safe to
// cancel it.
bool EditorWidget::editSafetyCheck()
{
  if (editor->unsavedChanges()) {
    // We already have unsaved changes, so assume that the safety
    // check has already passed or its warning dismissed.  (I do not
    // want to hit the disk for every edit operation.)
    return true;
  }

  if (!editor->file->hasStaleModificationTime()) {
    // No concurrent changes, safe to go ahead.
    return true;
  }

  // Prompt the user.
  QMessageBox box(this);
  box.setWindowTitle("File Changed");
  box.setText(toQString(stringb(
    "The file \"" << editor->file->filename << "\" has changed on disk.  "
    "Do you want to proceed with editing the in-memory contents anyway, "
    "overwriting the on-disk changes when you later save?")));
  box.addButton(QMessageBox::Yes);
  box.addButton(QMessageBox::Cancel);
  int ret = box.exec();
  if (ret == QMessageBox::Yes) {
    // Suppress warning later when we save.  Also, if the edit we
    // are about to do gets canceled for a different reason,
    // leaving us in the "clean" state after all, this refresh will
    // ensure we do not prompt the user a second time.
    editor->file->refreshModificationTime();

    // Go ahead with the edit.
    return true;
  }
  else {
    // Cancel the edit.
    return false;
  }
}


// EOF
