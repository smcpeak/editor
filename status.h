// status.h
// status display at the bottom of the editor window

#ifndef STATUS_H
#define STATUS_H

#include <QWidget>

class QLabel;
class QSizeGrip;


class StatusDisplay : public QWidget {
public:
  QLabel *position;           // cursor position
  QLabel *mode;               // mode pixmap
  QLabel *status;             // general status string
  QSizeGrip *corner;          // corner resize grippy

public:
  StatusDisplay(QWidget *parent = NULL);
  ~StatusDisplay();
};


#endif // STATUS_H
