// my-table-widget.h
// MyTableWidget class.

#ifndef MY_TABLE_WIDGET_H
#define MY_TABLE_WIDGET_H

#include <QTableWidget>


// Variant of QTableWidget with some customizations.
//
// Specifically: I treat the N and P keys like Down and Up arrow keys
// for easier keyboard navigation.
//
// This uses the "convenience" class combining a model and a view,
// rather than using a separate model and view.  Originally I used
// separate objects since I thought I would be able to take advantage of
// my existing change notification infrastructure for
// NamedTextDocumentList and simply relay to the Qt model change
// notifications, thereby saving the cost of building a copy of the
// table.
//
// However, the problem is the Qt model change design requires every
// change to be accompanied by a pre-change broadcast and a post-change
// broadcast.  In contrast, my own system only uses post-change
// broadcasts.  Rather than complicate by design by adding pre-change
// notifications, I have chosen to just pay the minor cost of having an
// extra copy of the table in memory.
class MyTableWidget : public QTableWidget {
private:     // funcs
  // Synthesize a keypress for the underlying QTableView.
  void synthesizeKey(int key, Qt::KeyboardModifiers modifiers);

public:      // funcs
  MyTableWidget(QWidget *parent = NULL);
  ~MyTableWidget();

  // Overridden QWidget methods.
  virtual void keyPressEvent(QKeyEvent *event) noexcept override;
};


#endif // MY_TABLE_WIDGET_H
