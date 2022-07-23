// pixmaps.h
// collect together the pixmaps used in the editor

#ifndef PIXMAPS_H
#define PIXMAPS_H

#include <qpixmap.h>         // QPixmap

class Pixmaps {
public:
  QPixmap icon;              // icon for editor windows
  QPixmap search;            // "search" status indicator
  QPixmap getReplace;        // get replacement text
  QPixmap replace;           // y/n "replace?" mode
  QPixmap connectionsIcon;   // Icon for ConnectionsDialog.

public:
  Pixmaps();
  ~Pixmaps();
};

// singleton
//
// TODO: Rename this to 'g_editorPixmaps'.
extern Pixmaps *pixmaps;

#endif // PIXMAPS_H
