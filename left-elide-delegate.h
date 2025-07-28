// left-elide-delegate.h
// `LeftElideDelegate`, a table rendering delegate that elides the left
// side of cell text.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LEFT_ELIDE_DELEGATE_H
#define EDITOR_LEFT_ELIDE_DELEGATE_H

#include "left-elide-delegate-fwd.h"   // fwds for this module

#include <QStyledItemDelegate>


// This delegate causes long text to be cut off on the left side rather
// than the right.  For example, "a long text string" might be drawn as
// "xt string" if that is all that fits.
class LeftElideDelegate : public QStyledItemDelegate {
private:     // methods
  QString leftElidedText(
    QString const &text,
    QFontMetrics const &fm,
    int width) const;

public:      // methods
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(
    QPainter *painter,
    QStyleOptionViewItem const &option,
    QModelIndex const &index) const override;
};


#endif // EDITOR_LEFT_ELIDE_DELEGATE_H
