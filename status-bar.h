// status-bar.h
// Status bar display at the bottom of the editor window.

#ifndef EDITOR_STATUS_BAR_H
#define EDITOR_STATUS_BAR_H

#include "status-bar-fwd.h"            // fwds for this module

#include <QWidget>

class QLabel;
class QSizeGrip;


class StatusBarDisplay : public QWidget {
public:      // data
  QLabel *m_cursor;          // cursor position
  QLabel *m_mode;            // mode pixmap
  QLabel *m_filename;        // current file name
  QSizeGrip *m_corner;       // corner resize grippy

public:      // methods
  StatusBarDisplay(QWidget *parent = NULL);
  ~StatusBarDisplay();

  // Set the text in 'm_filename'.  This should be used instead of
  // directly modifying it so the minimum width can be adjusted.
  void setFilenameText(QString newFilename);
};


#endif // EDITOR_STATUS_BAR_H
