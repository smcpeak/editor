/****************************************************************************
** Form interface generated from reading ui file 'gotoline.ui'
**
** Created: Sat Sep 27 03:02:14 2003
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#ifndef GOTOLINE_H
#define GOTOLINE_H

#include <qvariant.h>
#include <qdialog.h>
class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QLabel;
class QLineEdit;
class QPushButton;

class GotoLine : public QDialog
{ 
    Q_OBJECT

public:
    GotoLine( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~GotoLine();

    QLabel* TextLabel1;
    QPushButton* okButton;
    QPushButton* cancelButton;
    QLineEdit* lineNumber;

};

#endif // GOTOLINE_H
