// status-bar.cc
// code for status-bar.h

#include "status-bar.h"                // this module

#include "smbase/trace.h"              // TRACE

#include <QHBoxLayout>
#include <QLabel>
#include <QSizeGrip>


StatusBarDisplay::StatusBarDisplay(QWidget *parent)
  : QWidget(parent)
{
  int height = fontMetrics().height();
  TRACE("StatusBarDisplay", "height: " << height);
  this->setFixedHeight(height);

  QHBoxLayout *hb = new QHBoxLayout();
  hb->setContentsMargins(5, 0, 0, 0);

  m_cursor = new QLabel();
  m_cursor->setObjectName("m_cursor");
  m_cursor->setFixedWidth(80);
  //m_cursor->setBackgroundColor(QColor(0x60, 0x00, 0x80));   // purple
  //m_cursor->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //m_cursor->setLineWidth(1);
  hb->addWidget(m_cursor);

  m_mode = new QLabel();
  //m_mode->setPixmap(state->pixSearch);
  m_mode->setFixedWidth(65);
  hb->addWidget(m_mode);

  m_filename = new QLabel();
  m_filename->setObjectName("m_filename");

  // Disable the "autodetection" nonsense since the file name can be
  // almost any string, including things that look like HTML.
  m_filename->setTextFormat(Qt::PlainText);

  //m_filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //m_filename->setLineWidth(1);
  hb->addWidget(m_filename);

  hb->addStretch(1);

  // corner resize widget
  m_corner = new QSizeGrip(this);
  m_corner->setObjectName("m_corner");
  m_corner->setFixedSize(height, height);
  hb->addWidget(m_corner);

  this->setLayout(hb);
}


StatusBarDisplay::~StatusBarDisplay()
{
  // all the widgets are automatically deallocated
  // in ~QWidget
}


void StatusBarDisplay::setFilenameText(QString newFilename)
{
  m_filename->setText(newFilename);

  /* Sometimes the file name can be very long, and the normal behavior
     of QLabel is to set its minimum width according to the displayed
     text.  That would, in turn, force the window to potentially be very
     wide, which I don't want to require.

     Also, this interacts with the sceenshot1.ev test, since that needs
     a fairly narrow width, but the file name might be arbitrarily long
     depending on which directory the editor has been compiled in. */
  m_filename->setMinimumWidth(20);
}


// EOF
