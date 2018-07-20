// status.cc
// code for status.h

#include "status.h"       // this module

#include <QHBoxLayout>
#include <QLabel>
#include <QSizeGrip>


StatusDisplay::StatusDisplay(QWidget *parent)
  : QWidget(parent)
{
  this->setFixedHeight(20);

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
  //m_filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //m_filename->setLineWidth(1);
  hb->addWidget(m_filename);

  hb->addStretch(1);

  // corner resize widget
  m_corner = new QSizeGrip(this);
  m_corner->setObjectName("m_corner");
  m_corner->setFixedSize(20,20);
  hb->addWidget(m_corner);

  this->setLayout(hb);
}


StatusDisplay::~StatusDisplay()
{
  // all the widgets are automatically deallocated
  // in ~QWidget
}


// EOF
