// no-elide-delegate.h
// `NoElideDelegate`, a table cell delegate to disable "..." elision.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_NO_ELIDE_DELEGATE_H
#define EDITOR_NO_ELIDE_DELEGATE_H

#include "no-elide-delegate-fwd.h"     // fwds for this module

#include <QStyledItemDelegate>


// This delegate disables "..." text elision for its applicable cells.
class NoElideDelegate : public QStyledItemDelegate {
public:
  NoElideDelegate(QObject *parent = nullptr);

  // Intercept the paint routine to adjust the options, then pass to the
  // base class for actual painting.
  virtual void paint(
    QPainter *painter,
    QStyleOptionViewItem const &option,
    QModelIndex const &index) const override;
};


#endif // EDITOR_NO_ELIDE_DELEGATE_H
