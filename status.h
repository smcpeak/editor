// status.h
// status display at the bottom of the editor window

#ifndef STATUS_H
#define STATUS_H

#include <QWidget>

class QLabel;
class QSizeGrip;


class StatusDisplay : public QWidget {
public:      // data
  QLabel *m_cursor;          // cursor position
  QLabel *m_mode;            // mode pixmap
  QLabel *m_filename;        // current file name
  QSizeGrip *m_corner;       // corner resize grippy

public:      // methods
  StatusDisplay(QWidget *parent = NULL);
  ~StatusDisplay();

  // Set the text in 'm_filename'.  This should be used instead of
  // directly modifying it so the minimum width can be adjusted.
  void setFilenameText(QString newFilename);
};


#endif // STATUS_H
