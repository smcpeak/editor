// no-elide-delegate.cc
// Code for `no-elide-delegate` module.

// See license.txt for copyright and terms of use.

#include "no-elide-delegate.h"                  // this module


NoElideDelegate::NoElideDelegate(QObject *parent)
  : QStyledItemDelegate(parent)
{}


void NoElideDelegate::paint(
  QPainter *painter,
  QStyleOptionViewItem const &option,
  QModelIndex const &index) const
{
  QStyleOptionViewItem opt = option;

  // Normally, a delegate `paint` does this to populate the options with
  // information specific to the cell (such as its text), but it will
  // happen inside `QStyledItemDelegate::paint`, so we don't do it here.
  //initStyleOption(&opt, index);

  // Disable eliding.
  //
  // In addition to the obvious effect of stopping "..." from being
  // added, turning off elision has the effect of allowing the
  // combination of right-align (specified on the item) and no-word-wrap
  // (specified on the table) to combine to cause the text to be cropped
  // on the left rather than the right.  (That those two alone are
  // insufficient is arguably a bug in Qt.)
  opt.textElideMode = Qt::ElideNone;

  // The superclass can take care of the rest with the modified `opt`.
  QStyledItemDelegate::paint(painter, opt, index);
}


// EOF
