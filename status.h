// status.h
// status display at the bottom of the editor window

#ifndef STATUS_H
#define STATUS_H

#include <qhbox.h>       // QHBox

class QLabel;
class QSizeGrip;


class StatusDisplay : public QHBox {
public:
  QLabel *position;           // cursor position
  QLabel *mode;               // mode pixmap
  QLabel *status;             // general status string
  QSizeGrip *corner;          // corner resize grippy

public:
  StatusDisplay(QWidget *parent);
  ~StatusDisplay();
};


#endif // STATUS_H
