// left-elide-delegate.cc
// Code for `left-elide-delegate` module.

#include "left-elide-delegate.h"       // this module

#include "smqtutil/gdvalue-qrect.h"    // toGDValue(QRect)
#include "smqtutil/gdvalue-qstring.h"  // toGDValue(QString)
#include "smqtutil/qtguiutil.h"        // QPainterSaveRestore

#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/ordered-map-ops.h"    // gdv::GDVOrderedMap
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <QApplication>
#include <QPainter>
#include <QTextLayout>

using namespace gdv;

INIT_TRACE("left-elide-delegate");


// TODO: Understand this code.
void LeftElideDelegate::paint(
  QPainter *painter,
  QStyleOptionViewItem const &option,
  QModelIndex const &index) const
{
  // ?
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  QPainterSaveRestore painterSaveRestore(*painter);

  QString fullText = opt.text;
  opt.text.clear();  // Let us draw the text ourselves

  QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

  QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

  // Add some padding on the sides.
  textRect.adjust(+5, 0, -5, 0);

  QFontMetrics fm(opt.font);
  QString elided = leftElidedText(fullText, fm, textRect.width());

  QTextOption textOption(Qt::AlignRight | Qt::AlignVCenter);
  painter->drawText(textRect, elided, textOption);

  TRACE1("paint: " << GDValue(GDVOrderedMap{
    GDV_SKV_EXPR(textRect),
    GDV_SKV_EXPR(fullText),
    GDV_SKV_EXPR(elided),
  }));
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
