// my-table-view.h
// MyTableView class.

#ifndef MY_TABLE_VIEW_H
#define MY_TABLE_VIEW_H

#include <QTableView>


// Variant of QTableView with some customizations.
//
// Specifically: I treat the N and P keys like Down and Up arrow keys
// for easier keyboard navigation.
class MyTableView : public QTableView {
private:     // funcs
  // Synthesize a keypress for the underlying QTableView.
  void synthesizeKey(int key, Qt::KeyboardModifiers modifiers);

public:      // funcs
  MyTableView(QWidget *parent = NULL);
  ~MyTableView();

  // Overridden QWidget methods.
  virtual void keyPressEvent(QKeyEvent *event) noexcept override;
};


#endif // MY_TABLE_VIEW_H
