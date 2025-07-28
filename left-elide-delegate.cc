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

  // We can get the desired effect of left truncation by setting the
  // alignment to right and turning off wrapping.
  QTextOption textOption(Qt::AlignRight | Qt::AlignVCenter);
  textOption.setWrapMode(QTextOption::NoWrap);

  // Draw that text in the rectangle.
  painter->drawText(textRect, fullText, textOption);

  TRACE1("paint: " << GDValue(GDVOrderedMap{
    {
      "opt.widget name",
      opt.widget? toGDValue(opt.widget->objectName()) : GDValue()
    },
    GDV_SKV_EXPR(textRect),
    GDV_SKV_EXPR(fullText),
  }).asIndentedString());
}


// EOF
