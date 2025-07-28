// left-elide-delegate.cc
// Code for `left-elide-delegate` module.

#include "left-elide-delegate.h"       // this module

#include "smqtutil/gdvalue-qrect.h"    // toGDValue(QRect)
#include "smqtutil/gdvalue-qstring.h"  // toGDValue(QString)
#include "smqtutil/qtguiutil.h"        // QPainterSaveRestore
#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/ordered-map-ops.h"    // gdv::GDVOrderedMap
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <QApplication>
#include <QPainter>
#include <QTextLayout>

#include <utility>                     // std::move

using namespace gdv;

INIT_TRACE("left-elide-delegate");


// TODO: Understand this code.
void LeftElideDelegate::paint(
  QPainter *painter,
  QStyleOptionViewItem const &option,
  QModelIndex const &index) const
{
  // This pattern, copying the `option` parameter and then letting
  // `initStyleOption` further modify it, seems to be the standard way
  // to begin a delegate `paint` method.  The documentation is not very
  // clear, but `QStyledItemDelegate::paint` does it this way, and
  // `QStyledItemDelegate::initStyleOption` clearly updates its argument
  // without overwriting the whole thing.
  //
  // In particular, testing shows that `option.text` is an empty
  // string, and `initStyleOption` is what populates it.
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  // Nothing in this method directly alters the paint state, but the
  // documentation emphasizes the need to preserve it.
  QPainterSaveRestore painterSaveRestore(*painter);

  // Move the text into our own variable.  We will draw it, and do not
  // want `style->drawControl` to do so.
  QString fullText = std::move(opt.text);
  opt.text.clear();

  // Use the style of the containing widget if one is provided,
  // otherwise fall back on the application style.  In my testing,
  // `opt.widget` points at the `QTableWidget` that this delegate is
  // installed in.
  QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

  // Draw all elements except the text (because we cleared it).
  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

  // Get the rectangle to draw the text in.
  QRect textRect =
    style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

  // Add some padding on the sides.
  textRect.adjust(+5, 0, -5, 0);

  // Compute a shortened form of the text to draw.
  QFontMetrics fm(opt.font);
  QString elided = leftElidedText(fullText, fm, textRect.width());

  // Draw that text in the rectangle.
  QTextOption textOption(Qt::AlignRight | Qt::AlignVCenter);
  painter->drawText(textRect, elided, textOption);

  TRACE1("paint: " << GDValue(GDVOrderedMap{
    {
      "opt.widget name",
      opt.widget? toGDValue(opt.widget->objectName()) : GDValue()
    },
    GDV_SKV_EXPR(textRect),
    GDV_SKV_EXPR(fullText),
    GDV_SKV_EXPR(elided),
  }).asIndentedString());
}


// TODO: This is not how I want this to work.
QString LeftElideDelegate::leftElidedText(
  QString const &text,
  QFontMetrics const &fm,
  int width) const
{
  if (fm.horizontalAdvance(text) <= width)
    return text;

  const QString ellipsis = "...";
  int ellipsisWidth = fm.horizontalAdvance(ellipsis);
  int start = text.length() - 1;

  while (start > 0 && fm.horizontalAdvance(text.mid(start)) + ellipsisWidth < width)
    --start;

  return ellipsis + text.mid(start + 1);
}


// EOF
