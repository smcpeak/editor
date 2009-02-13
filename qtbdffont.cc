// qtbdffont.cc
// code for qtbdffont.h

#include "qtbdffont.h"                 // this module

#include "bdffont.h"                   // BDFFont
#include "bit2d.h"                     // Bit2d::Size

#include <qimage.h>                    // QImage
#include <qpaintdevice.h>              // QPaintDevice


// --------------------- QtBDFFont::Metrics -----------------------
QtBDFFont::Metrics::Metrics()
  : bbox(0,0,0,0),
    origin(0,0),
    offset(0,0)
{}


bool QtBDFFont::Metrics::isPresent() const
{
  // Must test offset as well as bbox because the glyph for ' ' can
  // have an empty bbox but still be present for its offset effect.
  if (bbox.width() == 0 && bbox.height() == 0 &&
      offset.x() == 0 && offset.y() == 0) {
    return false;
  }
  else {
    return true;
  }
}


// ------------------------- QtBDFFont --------------------------
QtBDFFont::QtBDFFont(BDFFont const &font)
  : glyphMask(),             // Null for now
    colorPixmap(),
    textColor(0,0,0),        // black
    allCharsBBox(0,0,0,0),
    metrics(font.glyphIndexLimit())
{
  // The main thing this constructor does is build the 'glyphMask'
  // bitmap and the 'metrics' array.  To do so, we pack the glyph
  // images into a rectangular bitmap.  In general, optimal packing is
  // NP-complete, and the benefit of efficiency here is not great, so
  // I use a completely naive strategy of putting them horizontally
  // adjacent, aligned at the bounding box top.
  //
  // However, it seems likely that with just a bit more work, I could
  // get a more efficient structure that tried to arrange glyphs in
  // several regular-height rows, with taller-than-usual glyphs off to
  // the side.  This would also have the advantage of reducing the
  // maximum dimension of the pixmap; the Qt docs explain that some
  // window systems have trouble with dimensions exceeding 4k.  But
  // I'll save those optimizations for another time.

  // maximum glyph height encountered so far
  int maxHeight = 0;

  // current horizontal position for the next glyph's left edge
  int currentX = 0;

  // Pass 1: Compute 'metrics'.
  for (int i=0; i < metrics.size(); i++) {
    BDFFont::Glyph const *glyph = font.getGlyph(i);
    if (!glyph) {
      // metrics[i] should already be zeroed
      continue;
    }
    BDFFont::GlyphMetrics const &gmet = glyph->metrics;

    // place glyph 'i' here
    metrics[i].bbox = QRect(currentX, 0,
                            gmet.bbSize.x, gmet.bbSize.y);

    // If gmet.bbOffset had value (0,0), then the origin would be the
    // lower-left corner of the glyph bbox, which is (currentX,
    // gmet.bbSize.y-1) in the 'glyphMask' bitmap.
    //
    // From there, a positive offset of gmet.bbOffset.x is interpreted
    // as moving the bbox to the right, which is equivalent to moving
    // the origin left, so we subtract.
    //
    // A positive offset of gmet.bbOffset.y is interpreted as moving
    // the bbox *up* in the coordinate system of
    // BDFFont::GlyphMetrics, which is equivalent to moving the origin
    // down, so we add (since down is positive in the coordinate
    // system of 'glyphMask').
    metrics[i].origin = QPoint(currentX - gmet.bbOffset.x,
                               gmet.bbSize.y-1 + gmet.bbOffset.y);
                     
    // Get movement offset, which might come from 'font'.
    point dWidth = gmet.hasDWidth()?
                     gmet.dWidth :
                     font.metrics.dWidth;

    // Origin movement offset.  Same as 'dWidth', except again the 'y'
    // axis inverted.  Except, you'd never know, since in practice it
    // will always be 0.
    metrics[i].offset = QPoint(dWidth.x, -dWidth.y);

    // bump variables involved in packing calculation
    maxHeight = max(maxHeight, gmet.bbSize.y);
    currentX += gmet.bbSize.x;

    // Update 'allCharsBBox'.  This call reads from 'metrics[i]'.
    allCharsBBox |= getCharBBox(i);
  }

  // Allocate an image with the same size as 'glyphMask' will
  // ultimately be.  I use a QImage here because I'm going to use the
  // slow method of copying individual pixels, for now, and a
  // QPixmap/QBitmap is very slow at accessing individual pixels.
  QImage tempMask(currentX,                      // width
                  maxHeight,                     // height
                  1,                             // depth
                  2,                             // colors
                  QImage::LittleEndian);         // bit order; I don't care
  tempMask.fill(0);

  // Pass 2: Copy the glyph images using the positions calculated
  // above.
  for (int i=0; i < metrics.size(); i++) {
    BDFFont::Glyph const *glyph = font.getGlyph(i);
    if (!glyph) {
      continue;
    }

    if (!glyph->bitmap) {
      continue;      // nothing to copy
    }
    xassert(glyph->bitmap->Size() == glyph->metrics.bbSize);

    // Copy the pixels one by one.
    //
    // This could be made much faster, but doing so requires a lot of
    // low-level bit manipulation, and also some experimentation
    // because the Qt docs are a little vague about some of the
    // required details.
    for (int y=0; y < glyph->metrics.bbSize.y; y++) {
      for (int x=0; x < glyph->metrics.bbSize.x; x++) {
        if (glyph->bitmap->get(point(x,y))) {
          tempMask.setPixel(metrics[i].bbox.x() + x,
                            metrics[i].bbox.y() + y,
                            1);
        }
      }
    }
  }

  // Create 'glyphMask' from 'tempMask'.  This allocates, converts the
  // data from QImage to QBitmap, and copies it to the window system,
  // all hidden behind the innocuous '='.
  glyphMask = tempMask;

  // Create 'colorPixmap' and fill it with 'textColor'.
  colorPixmap.resize(glyphMask.size());
  colorPixmap.fill(textColor);

  // Associate the mask.
  colorPixmap.setMask(glyphMask);
}


QtBDFFont::~QtBDFFont()
{}


int QtBDFFont::maxValidChar() const
{
  int ret = metrics.size() - 1;
  while (ret >= 0 && !hasChar(ret)) {
    ret--;
  }
  return ret;
}


bool QtBDFFont::hasChar(int index) const
{
  if (0 <= index && index < metrics.size()) {
    return metrics[index].isPresent();
  }
  else {
    return false;
  }
}


QRect QtBDFFont::getCharBBox(int index) const
{
  if (hasChar(index)) {
    QRect ret(metrics[index].bbox);
    ret.moveBy(-metrics[index].origin.x(), -metrics[index].origin.y());
    return ret;
  }
  else {
    return QRect(0,0,0,0);
  }
}


QPoint QtBDFFont::getCharOffset(int index) const
{
  if (hasChar(index)) {
    return metrics[index].offset;
  }
  else {
    return QPoint(0,0);
  }
}


void QtBDFFont::setColor(QColor const &color)
{
  if (textColor != color) {
    textColor = color;
    colorPixmap.fill(textColor);
  }
}


void QtBDFFont::drawChar(QPaintDevice *dest, QColor const &color,
                         QPoint pt, int index)
{
  if (!hasChar(index)) {
    return;
  }
  Metrics const &met = metrics[index];

  setColor(color);

  // Upper-left corner of rectangle to copy, in the 'dest' coords.
  pt -= (met.origin - met.bbox.topLeft());

  // Copy the image.
  bitBlt(dest,                         // dest device
         pt,                           // upper-left of dest rectangle
         &colorPixmap,                 // source device (and mask)
         met.bbox,                     // source rectangle
         Qt::CopyROP);                 // blit operation
}


void drawString(QtBDFFont &font, QPaintDevice *dest,
                QColor const &color, QPoint pt, rostring str)
{
  for (char const *p = str.c_str(); *p; p++) {
    // Interpret each byte as a character index, unsigned
    // because no encoding system uses negative indices.
    int charIndex = (unsigned char)*p;

    font.drawChar(dest, color, pt, charIndex);
    pt += font.getCharOffset(charIndex);
  }
}


QRect getStringBBox(QtBDFFont &font, rostring str)
{
  // Loop to search for the first valid glyph.
  for (char const *p = str.c_str(); *p; p++) {
    int charIndex = (unsigned char)*p;

    if (font.hasChar(charIndex)) {
      // Begin actual bbox calculation.

      // Accumulated bbox.  Start with first character's bbox.
      QRect ret(font.getCharBBox(charIndex));

      // Virtual cursor; where to place next glyph.
      QPoint cursor = font.getCharOffset(charIndex);

      // Add the bboxes for subsequent characters.
      for (p++; *p; p++) {
        int charIndex = (unsigned char)*p;
        QRect glyphBBox = font.getCharBBox(charIndex);
        glyphBBox.moveBy(cursor.x(), cursor.y());
        ret |= glyphBBox;

        cursor += font.getCharOffset(charIndex);
      }

      return ret;
    }
  }

  // No valid glyphs.
  return QRect(0,0,0,0);
}


void drawCenteredString(QtBDFFont &font, QPaintDevice *dest,
                        QColor const &color, QPoint center,
                        rostring str)
{
  // Calculate a bounding rectangle for the entire string.
  QRect bbox = getStringBBox(font, str);

  // Upper-left of desired rectangle.
  QPoint pt = center - QPoint(bbox.width() / 2, bbox.height() / 2);

  // Origin point within the rectangle.
  pt -= bbox.topLeft();

  // Draw it.
  drawString(font, dest, color, pt, str);
}


// -------------------------- test code -----------------------------
#ifdef TEST_QTBDFFONT

#include "test.h"                      // ARGS_MAIN

#include <qapplication.h>              // QApplication
#include <qlabel.h>                    // QLabel
#include <qpainter.h>                  // QPainter

ARGS_MAIN


// Test whether 'qfont' has the same information as 'font'.  Throw
// an exception if not.
static void compare(BDFFont const &font, QtBDFFont &qfont)
{
  xassert(font.maxValidGlyph() == qfont.maxValidChar());

  int glyphCount = 0;

  // Iterate over all potentially valid indices.
  for (int charIndex=0; charIndex <= font.maxValidGlyph(); charIndex++) {
    #define CHECK_EQUAL(a,b)                                       \
      if ((a) == (b)) {                                            \
        /* fine */                                                 \
      }                                                            \
      else {                                                       \
        xfailure(stringb("expected '" #a "' (" << (a) <<           \
                         ") to equal '" #b "' (" << (b) << ")"));  \
      }

    try {
      // Check for consistent presence in both.
      BDFFont::Glyph const *fontGlyph = font.getGlyph(charIndex);
      CHECK_EQUAL(fontGlyph!=NULL, qfont.hasChar(charIndex));
      if (fontGlyph == NULL) {
        continue;
      }
      glyphCount++;

      // Bounding box.
      QRect bbox = qfont.getCharBBox(charIndex);
      {
        // Compare to bbox from 'font'.  This basically repeats logic
        // from the QtBDFFont constructor; oh well.
        CHECK_EQUAL(bbox.width(), fontGlyph->metrics.bbSize.x);
        CHECK_EQUAL(bbox.height(), fontGlyph->metrics.bbSize.y);

        CHECK_EQUAL(bbox.left(), fontGlyph->metrics.bbOffset.x);
        
        // This formula is complicated because the meaning of
        // increasing 'y' values are reversed, and that in turn means
        // that the "top" point is a different corner than the
        // "offset" corner.
        //
        // Representative example for lowercase 'j', where X is
        // a drawn (black) pixel and O is the origin point.
        //
        // bbox coords
        // -----------
        //     -4       X   ^
        //     -3           |
        //     -2       X   |
        //     -1       X   |height=7
        //      0    O  X   |
        //      1    X  X   |
        //      2     XX    V
        //
        // fontGlyph->metrics.bbSize.y and bbox.height() are 7.
        //
        // fontGlyph->metrics.bbOffset.y is -2 since the bbox bottom
        // is 2 pixels below the origin point.
        //
        // bbox.top() is -4, which is (-7) + 1 + (-(-2)).
        //
        CHECK_EQUAL(bbox.top(), (-fontGlyph->metrics.bbSize.y) + 1 +
                                (-fontGlyph->metrics.bbOffset.y));
      }
      
      // Offset.
      QPoint offset = qfont.getCharOffset(charIndex);
      {
        // Compare to 'font'.
        point dWidth = fontGlyph->metrics.hasDWidth()?
                         fontGlyph->metrics.dWidth :
                         font.metrics.dWidth;
                         
        CHECK_EQUAL(offset.x(), dWidth.x);
        CHECK_EQUAL(offset.y(), -dWidth.y);
      }
      
      // Now the interesting part: make a temporary pixmap, render
      // the glyph on to it, then compare it to what is in 'font'.
              
      // The temporary pixmap will have 10 pixels of margin around the
      // sides so that we can detect if the renderer is drawing pixels
      // outside the claimed bounding box.
      enum { MARGIN = 10 };
      QPixmap pixmap(bbox.width() + MARGIN*2, bbox.height() + MARGIN*2);
      pixmap.fill(Qt::white);

      // Calculate the location of the origin pixel for the glyph if I
      // want the top-left corner of the bbox to go at (10,10).
      QPoint origin = QPoint(MARGIN,MARGIN) - bbox.topLeft();

      // Render the glyph.
      qfont.drawChar(&pixmap, Qt::black, origin, charIndex);

      // Now, convert the QPixmap into a QImage to allow fast access
      // to individual pixels.
      QImage image = pixmap.convertToImage();

      // Examine every pixel in 'image'.
      for (int y=0; y < image.height(); y++) {
        for (int x=0; x < image.width(); x++) {
          try {
            // Is the pixel black or white?
            bool isBlack;
            {
              QRgb rgb = image.pixel(x,y);

              // first, make sure 'rgb' is either black or white,
              // mostly to confirm my understanding of how this API
              // works
              xassert(qBlue(rgb) == qRed(rgb));
              xassert(qBlue(rgb) == qGreen(rgb));
              xassert(qBlue(rgb) == 0 || qBlue(rgb) == 255);
              
              isBlack = (qBlue(rgb) == 0);
            }

            // Margin area?
            if (x < MARGIN ||
                y < MARGIN ||
                x >= image.width() - MARGIN ||
                y >= image.height() - MARGIN) {
              CHECK_EQUAL(isBlack, false);
              continue;
            }

            // Which pixel does this correspond to in
            // 'fontGlyph->bitmap'?
            point corresp(x - MARGIN, y - MARGIN);
            CHECK_EQUAL(isBlack, fontGlyph->bitmap->get(corresp));
          }

          catch (xBase &exn) {
            exn.prependContext(stringb("pixel (" << x << ", " << y << ")"));
            throw;
          }
        } // loop over 'x'
      } // loop over 'y'
    }

    catch (xBase &x) {
      x.prependContext(stringb("index " << charIndex));
      throw;
    }

    #undef CHECK_EQUAL
  } // loop over 'charIndex'
  
  cout << "successfully compared " << glyphCount << " glyphs\n";
}


void entry(int argc, char **argv)
{
  BDFFont font;
  parseBDFFile(font, "fonts/editor14r.bdf");

  QApplication app(argc, argv);

  QtBDFFont qfont(font);
  compare(font, qfont);

  QLabel widget(NULL /*parent*/);
  widget.resize(300,100);

  QPixmap pixmap(300,100);
  pixmap.fill(Qt::white);

  {
    QPainter painter(&pixmap);
    painter.drawText(50,20, "QPainter::drawText");
  }

  drawString(qfont, &pixmap, Qt::black, QPoint(50,50),
             "drawString(QtBDFFont &)");

  widget.setPixmap(pixmap);

  app.setMainWidget(&widget);
  widget.show();

  app.exec();
}

#endif // TEST_QTBDFFONT
