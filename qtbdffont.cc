// qtbdffont.cc
// code for qtbdffont.h

#include "qtbdffont.h"                 // this module

#include "smbase/bdffont.h"            // BDFFont
#include "smbase/bit2d.h"              // Bit2d::Size

#include <qimage.h>                    // QImage
#include <qpaintdevice.h>              // QPaintDevice


// --------------------- QtBDFFont::Metrics -----------------------
QtBDFFont::Metrics::Metrics()
  : bbox(0,0,0,0),
    origin(0,0),
    offset(0,0)
{}


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

    // Origin movement offset.  Same as 'gmet.dWidth', except again
    // the 'y' axis inverted.  Except, you'd never know, since in
    // practice it will always be 0.
    metrics[i].offset = QPoint(gmet.dWidth.x, -gmet.dWidth.y);

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


bool QtBDFFont::hasGlyph(int index) const
{
  if (0 <= index && index < metrics.size()) {
    // Must test offset as well as width because the glyph for ' ' can
    // have an empty bbox but still be present for its offset effect.
    return metrics[index].bbox.width() > 0 &&
           metrics[index].offset.x() != 0;  // cheat a little on this test
  }
  else {
    return false;
  }
}


QRect QtBDFFont::getCharBBox(int index) const
{
  if (hasGlyph(index)) {
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
  if (hasGlyph(index)) {
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


void QtBDFFont::drawCharacter(QPaintDevice *dest, QColor const &color,
                              QPoint pt, int index)
{
  if (!hasGlyph(index)) {
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
    font.drawCharacter(dest, color, pt, *p);
    pt += font.getCharOffset(*p);
  }
}


QRect getStringBBox(QtBDFFont &font, rostring str)
{
  // Loop to search for the first valid glyph.
  for (char const *p = str.c_str(); *p; p++) {
    if (font.hasGlyph(*p)) {
      // Begin actual bbox calculation.

      // Accumulated bbox.  Start with first character's bbox.
      QRect ret(font.getCharBBox(*p));

      // Virtual cursor; where to place next glyph.
      QPoint cursor = font.getCharOffset(*p);

      // Add the bboxes for subsequent characters.
      for (p++; *p; p++) {
        QRect glyphBBox = font.getCharBBox(*p);
        glyphBBox.moveBy(cursor.x(), cursor.y());
        ret |= glyphBBox;

        cursor += font.getCharOffset(*p);
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

#include "smbase/test.h"               // ARGS_MAIN

#include <qapplication.h>              // QApplication
#include <qlabel.h>                    // QLabel
#include <qpainter.h>                  // QPainter

ARGS_MAIN

void entry(int argc, char **argv)
{
  BDFFont font;
  parseBDFFile(font, "fonts/editor14r.bdf");

  QApplication app(argc, argv);

  QLabel widget(NULL /*parent*/);
  widget.resize(300,100);

  QPixmap pixmap(300,100);
  pixmap.fill(Qt::white);

  {
    QPainter painter(&pixmap);
    painter.drawText(50,20, "QPainter::drawText");
  }
  
  QtBDFFont font2(font);
  drawString(font2, &pixmap, Qt::black, QPoint(50,50),
             "drawString(QtBDFFont&)");

  widget.setPixmap(pixmap);

  app.setMainWidget(&widget);
  widget.show();

  app.exec();
}

#endif // TEST_QTBDFFONT
