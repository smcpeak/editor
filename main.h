// main.h
// Editor module with main() function.

#ifndef MAIN_H
#define MAIN_H

#include "editor-window.h"             // EditorWindow

#include "objlist.h"                   // ObjList
#include "pixmaps.h"                   // Pixmaps
#include "file-td.h"                   // FileTextDocument
#include "file-td-list.h"              // FileTextDocumentList

#include <QApplication>
#include <QProxyStyle>


// Define my look and feel overrides.
class EditorProxyStyle : public QProxyStyle {
public:
  int pixelMetric(
    PixelMetric metric,
    const QStyleOption *option = NULL,
    const QWidget *widget = NULL) const override;
};


// global state of the editor: files, windows, etc.
class GlobalState : public QApplication {
  Q_OBJECT

public:       // data
  // pixmap set
  Pixmaps pixmaps;

  // List of open files.  Never empty (see FileTextDocumentList).
  //
  // TODO: Rename to 'documentList'.
  FileTextDocumentList fileDocuments;

  // currently open editor windows; nominally, once the
  // last one of these closes, the app quits
  ObjList<EditorWindow> windows;

private:      // funcs
  bool hotkeyAvailable(int key) const;

public slots:
  void focusChangedHandler(QWidget *from, QWidget *to);

public:       // funcs
  // intent is to make one of these in main()
  GlobalState(int argc, char **argv);
  ~GlobalState();

  // To run the app, use the 'exec()' method, inherited
  // from QApplication.

  // Create an empty "untitled" file, add it to the set of documents,
  // and return it.
  FileTextDocument *createNewFile();

  // Return true if any file document has the given file name.
  bool hasFileWithName(string const &fname) const;

  // Return true if any file document has the given title.
  bool hasFileWithTitle(string const &title) const;

  // Calculate a minimal suffix of path components in 'filename' that
  // forms a title not shared by any open file.
  string uniqueTitleFor(string const &filename);

  // Open a new editor window.
  EditorWindow *createNewWindow(FileTextDocument *initFile);

  // Add 'f' to the set of file documents.
  void trackNewDocumentFile(FileTextDocument *f);

  // Remove 'f' from the set of file documents and deallocate it.
  // This will tell all of the editor widgets to forget about it
  // first so they switch to some other file.
  void deleteDocumentFile(FileTextDocument *f);
};


#endif // MAIN_H
