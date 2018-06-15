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
  hb->setContentsMargins(0, 0, 0, 0);

  this->position = new QLabel();
  this->position->setObjectName("cursor position label");
  this->position->setFixedWidth(80);
  //this->position->setBackgroundColor(QColor(0x60, 0x00, 0x80));   // purple
  //this->position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //this->position->setLineWidth(1);
  hb->addWidget(this->position);

  this->mode = new QLabel();
  //this->mode->setPixmap(state->pixSearch);
  this->mode->setFixedWidth(65);
  hb->addWidget(this->mode);

  this->status = new QLabel();
  this->status->setObjectName("filename label");
  //this->filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //this->filename->setLineWidth(1);
  hb->addWidget(this->status);

  hb->addStretch(1);

  // corner resize widget
  this->corner = new QSizeGrip(this);
  this->corner->setObjectName("corner grip");
  this->corner->setFixedSize(20,20);
  hb->addWidget(this->corner);

  this->setLayout(hb);
}


StatusDisplay::~StatusDisplay()
{
  // all the widgets are automatically deallocated
  // in ~QWidget
}


// EOF
