/****************************************************************************
** Form implementation generated from reading ui file 'gotoline.ui'
**
** Created: Sat Sep 27 03:02:14 2003
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#include "gotoline.h"

#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/* 
 *  Constructs a GotoLine which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
GotoLine::GotoLine( QWidget* parent,  const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "GotoLine" );
    resize( 211, 79 ); 
    setCaption( tr( "Goto Line" ) );
    setFocusPolicy( QDialog::NoFocus );

    TextLabel1 = new QLabel( this, "TextLabel1" );
    TextLabel1->setGeometry( QRect( 10, 10, 80, 20 ) ); 
    TextLabel1->setText( tr( "Line number:" ) );

    okButton = new QPushButton( this, "okButton" );
    okButton->setGeometry( QRect( 10, 40, 90, 28 ) ); 
    okButton->setText( tr( "Ok" ) );
    okButton->setDefault( TRUE );

    cancelButton = new QPushButton( this, "cancelButton" );
    cancelButton->setGeometry( QRect( 110, 40, 90, 28 ) ); 
    cancelButton->setText( tr( "Cancel" ) );

    lineNumber = new QLineEdit( this, "lineNumber" );
    lineNumber->setGeometry( QRect( 90, 10, 110, 22 ) ); 

    // signals and slots connections
    connect( cancelButton, SIGNAL( clicked() ), this, SLOT( reject() ) );
    connect( okButton, SIGNAL( clicked() ), this, SLOT( accept() ) );

    // tab order
    setTabOrder( lineNumber, okButton );
    setTabOrder( okButton, cancelButton );
}

/*  
 *  Destroys the object and frees any allocated resources
 */
GotoLine::~GotoLine()
{
    // no need to delete child widgets, Qt does it all for us
}

