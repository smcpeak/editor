// status.h
// status display at the bottom of the editor window

#ifndef STATUS_H
#define STATUS_H

#include <QWidget>

class QLabel;
class QSizeGrip;


class StatusDisplay : public QWidget {
public:
  QLabel *m_cursor;          // cursor position
  QLabel *m_mode;            // mode pixmap
  QLabel *m_filename;        // current file name
  QSizeGrip *m_corner;       // corner resize grippy

public:
  StatusDisplay(QWidget *parent = NULL);
  ~StatusDisplay();
};


#endif // STATUS_H
