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

// With regard to individual character indices, this module takes the
// same approach as the 'bdffont' module upon which it is built:
// characters are named using 'int' and no assumptions are made about
// the meaning of characters.
//
// However, the routines such as 'drawString' that accept a 'string'
// treat each byte as a character index, and thus are limited to
// character encoding systems with 256 characters or less.

// 2009-06-13: Originally, the design called for always drawing with a
// transparent background.  However, I have found that, at least on my
// machine, blitting with a mask is 20x slower than blitting without a
// mask, and that causes the display to be noticeable sluggish.  So,
// I've now switched it so it always draws an opaque background.  As
// such, the interface has changed so that the user must specify both
// the foreground and background colors when creating the QtBDFFont
// object, since changing the colors is as slow as making it in the
// first place.
//
// NOTE: The "opaque background" only covers the glyph's bounding box.
// So, in practice, clients must still erase the background area first
// themselves before rendering text.  This also precludes kerning.  I
// wish I had a better answer here...

#ifndef QTBDFFONT_H
#define QTBDFFONT_H

#include "array.h"                     // GrowArray
#include "str.h"                       // rostring

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

    // Return true if this glyph is present, false if missing.
    bool isPresent() const;
  };

private:     // data
  // Pixmap containing all the rendered font glyphs, packed together
  // such that no two bounding boxes overlap.  Other packing
  // characteristics are implementation details.
  QPixmap glyphs;

  // Relative to the origin, the minimal bounding box that encloses
  // every glyph in the font.
  QRect allCharsBBox;

  // Map from character index to associated metrics.  It does not
  // "grow"; I use GrowArray for its bounds checking.
  GrowArray<Metrics> metrics;

public:      // funcs
  // This makes a copy of all required data in 'font'; 'font' can be
  // destroyed afterward.
  QtBDFFont(BDFFont const &font,
            QColor const &fgColor, QColor const &bgColor);
  ~QtBDFFont();

  // Return the maximum valid character index, or -1 if there are no
  // valid indices.
  int maxValidChar() const;

  // Return true if there is a glyph with the given index.
  bool hasChar(int index) const;

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

  // Render a single character at 'pt'.  Renders the entire *bounding
  // box* opaquely, using the foreground and background colors
  // specified in the constructor call.
  //
  // If there is no glyph with the given index, this is a no-op.
  void drawChar(QPaintDevice *dest, QPoint pt, int index);
};


// Draw a string at 'pt'.
//
// The individual characters in 'str' are interpreted as 'unsigned
// char' for purposes of extracting a character index.  (See note at
// top of file.)
void drawString(QtBDFFont &font, QPaintDevice *dest,
                QPoint pt, rostring str);


// For an entire string, calculate a bounding rectangle, assuming the
// origin is at (0,0).  As with 'getCharBBox', the top of the
// resulting rectangle will usually be negative.  Returns (0,0,0,0) if
// none of the glyphs in 'str' are present.
QRect getStringBBox(QtBDFFont &font, rostring str);


// Draw a string centered both horizontally and vertically about
// the given point, according to the glyph bbox metrics.
void drawCenteredString(QtBDFFont &font, QPainter *dest,
                        QPoint center, rostring str);


#endif // QTBDFFONT_H
