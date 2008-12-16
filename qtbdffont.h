// qtbdffont.h
// module for writing to QPaintDevices using BDF fonts
//
// This module relies in the smbase/bdffont.h module to obtain font
// glyphs.  It then copies that font information into a format
// suitable for efficiently drawing with it with Qt.
//
// It plays a role similar to QFont and QPainter::drawText, except
// that it does not rely on the underlying window system for any font
// or text rendering services.

#ifndef QTBDFFONT_H
#define QTBDFFONT_H

#include "smbase/array.h"              // GrowArray
#include "smbase/str.h"                // rostring

#include <qbitmap.h>                   // QBitmap, QPixmap
#include <qcolor.h>                    // QColor
#include <qpoint.h>                    // QPoint
#include <qrect.h>                     // QRect

class BDFFont;                         // smbase/bdffont.h
class QPaintDevice;                    // qpaintdevice.h


// Store a font in a form suitable for drawing.
//
// In this class, X values increase going right, Y values increase
// going down.
//
// Some methods are marked 'const', but (for now) there is no useful
// notion of constness for this class, since it is semantically
// immutable.
class QtBDFFont {
private:     // types
  // Metrics about a single glyph.  Missing glyphs have all values set
  // to 0.
  class Metrics {
  public:    // data
    // Glyph bounding box in 'glyphMask' bitmap.
    QRect bbox;

    // Location of the glyph origin point in 'glyphMask'.  Not
    // necessarily inside 'bbox', nor even inside the dimensions of
    // 'glyphMask'.
    QPoint origin;

    // Relative amount by which to move the drawing point after
    // drawing this glyph.
    QPoint offset;

  public:
    Metrics();
  };

private:     // data
  // Bitmap containing all the font glyphs, packed together such that
  // no two overlap.  Other packing characteristics are implementation
  // details.
  QBitmap glyphMask;

  // A pixmap entirely filled with the current text color.  It is the
  // same size as 'glyphMask', and has 'glyphMask' as its mask.
  QPixmap colorPixmap;

  // Current text color.  'colorPixmap' is filled with it.  This is
  // changed when an attempt is made to draw a different color.
  QColor textColor;

  // Relative to the origin, the minimal bounding box that encloses
  // all glyphs in the font.
  QRect allCharsBBox;

  // Map from character index to associated metrics.  It does not
  // "grow"; I use GrowArray for its bounds checking.
  GrowArray<Metrics> metrics;

private:     // funcs
  // Make sure the current text color matches 'color'.
  void setColor(QColor const &color);

public:      // funcs
  // This makes a copy of all required data in 'font'; 'font' can be
  // destroyed afterward
  QtBDFFont(BDFFont const &font);
  ~QtBDFFont();

  // Return true if there is a glyph with the given index.
  bool hasGlyph(int index) const;

  // Return the origin-relative bounding box of a glyph.  Will return
  // (0,0,0,0) if the glyph is missing.
  //
  // Note that any glyph with pixels above the origin point (which is
  // most of them) will have a negative value for the 'top' value of
  // the rectangle, because this class's coordinate system has Y
  // increasing going down.
  QRect getCharBBox(int index) const;

  // Return the origin-relative minimal bounding box for all glyphs.
  QRect const &getAllCharsBBox() const { return allCharsBBox; }

  // Return the offset by which the origin should move after drawing
  // a given glyph.  Returns (0,0) if the glyph is missing.
  QPoint getCharOffset(int index) const;

  // Render a single character at 'pt'.  The text is drawn with
  // 'color'.
  //
  // If there is no glyph with the given index, this is a no-op.
  void drawCharacter(QPaintDevice *dest, QColor const &color,
                     QPoint pt, int index);
};


// Draw a string at 'pt'.
void drawString(QtBDFFont &font, QPaintDevice *dest,
                QColor const &color, QPoint pt, rostring str);


// For an entire string, calculate a bounding rectangle, assuming the
// origin is at (0,0).  As with 'getCharBBox', the top of the
// resulting rectangle will usually be negative.  Returns (0,0,0,0) if
// none of the glyphs in 'str' are present.
QRect getStringBBox(QtBDFFont &font, rostring str);


// Draw a string centered both horizontally and vertically about
// the given point, according to the glyph bbox metrics.
void drawCenteredString(QtBDFFont &font, QPainter *dest,
                        QColor const &color, QPoint center,
                        rostring str);

#endif // QTBDFFONT_H
