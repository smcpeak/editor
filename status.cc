// status.cc
// code for status.h

#include "status.h"       // this module

#include <qlabel.h>       // QLabel
#include <qsizegrip.h>    // QSizeGrip


StatusDisplay::StatusDisplay(QWidget *parent)
  : QHBox(parent, "status area")
{
  setFixedHeight(20);
  
  position = new QLabel(this, "cursor position label");
  position->setFixedWidth(60);
  //position->setBackgroundColor(QColor(0x60, 0x00, 0x80));   // purple
  //position->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //position->setLineWidth(1);

  mode = new QLabel(this);
  //mode->setPixmap(state->pixSearch);
  mode->setFixedWidth(65);  

  status = new QLabel(this, "filename label");
  //filename->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  //filename->setLineWidth(1);

  // corner resize widget
  QSizeGrip *corner = new QSizeGrip(this, "corner grip");
  corner->setFixedSize(20,20);
}


StatusDisplay::~StatusDisplay()
{
  // all the widgets are automatically deallocated
  // in ~QWidget
}


// EOF
