// file-td-list.cc
// code for file-td-list.h

#include "file-td-list.h"              // this module

// smbase
#include "macros.h"                    // Restorer
#include "stringset.h"                 // StringSet
#include "strtokp.h"                   // StrtokParse
#include "strutil.h"                   // dirname
#include "trace.h"                     // TRACE

// libc++
#include <set>                         // std::set

using std::set;


// Run self-check in debug mode.
#ifndef NDEBUG
#  define SELF_CHECK() this->selfCheck() /* user ; */
#else
#  define SELF_CHECK() ((void)0) /* user ; */
#endif


// --------------------- FileTextDocumentList -----------------------
FileTextDocumentList::FileTextDocumentList()
  : m_observers(),
    m_iteratingOverObservers(false),
    m_fileDocuments()
{
  this->createUntitledFile();
  SELF_CHECK();
}


FileTextDocumentList::~FileTextDocumentList()
{
  // Do this explicitly for clarity.
  m_observers.removeAll();
  m_fileDocuments.deleteAll();
}


// True if 's' contains 't'.
//
// This is a candidate for moving someplace more general.
template <class T>
bool contains(set<T> const &s, T const &t)
{
  return s.find(t) != s.end();
}

template <class T>
void insertUnique(set<T> &s, T const &t)
{
  xassert(!contains(s, t));
  s.insert(t);
}


void FileTextDocumentList::selfCheck() const
{
  xassert(m_fileDocuments.isNotEmpty());

  // Sets of attributes seen, to check for uniqueness.
  StringSet filenames;
  StringSet titles;
  set<int> hotkeyDigits;
  set<int> windowMenuIds;

  for (int i=0; i < m_fileDocuments.length(); i++) {
    FileTextDocument const *d = m_fileDocuments[i];
    xassert(!d->m_filename.isempty());
    filenames.addUnique(d->m_filename);
    xassert(!d->m_title.isempty());
    filenames.addUnique(d->m_title);
    if (d->hasHotkey()) {
      insertUnique(hotkeyDigits, d->getHotkeyDigit());
    }
    insertUnique(windowMenuIds, d->m_windowMenuId);
  }
}


int FileTextDocumentList::numFiles() const
{
  return m_fileDocuments.length();
}


FileTextDocument *FileTextDocumentList::getFileAt(int index)
{
  return const_cast<FileTextDocument*>(this->getFileAtC(index));
}

FileTextDocument const *FileTextDocumentList::getFileAtC(int index) const
{
  return m_fileDocuments[index];
}


bool FileTextDocumentList::hasFile(FileTextDocument const *file) const
{
  return getFileIndex(file) >= 0;
}


int FileTextDocumentList::getFileIndex(FileTextDocument const *file) const
{
  return m_fileDocuments.indexOf(file);
}


void FileTextDocumentList::addFile(FileTextDocument *file)
{
  TRACE("file-td-list", "addFile: " << file->m_filename);
  xassert(!this->hasFile(file));

  // Assign title if necessary.
  if (file->m_title.isempty() || this->findFileByTitle(file->m_title)) {
    file->m_title = this->computeUniqueTitle(file->m_filename);
  }

  // Assign hotkey if necessary.
  if (!file->hasHotkey() ||
      this->findFileByHotkey(file->getHotkeyDigit()) != NULL) {
    int digit;
    if (this->computeUniqueHotkey(digit)) {
      file->setHotkeyDigit(digit);
    }
    else {
      file->clearHotkey();
    }
  }

  m_fileDocuments.append(file);

  this->notifyAdded(file);
  SELF_CHECK();
}


void FileTextDocumentList::removeFile(FileTextDocument *file)
{
  TRACE("file-td-list", "removeFile: " << file->m_filename);

  // If we make an untitled file, allow it to take the same hotkey.
  file->clearHotkey();

  if (this->numFiles() == 1) {
    // Ensure we will not end up with an empty list.
    this->createUntitledFile();
  }

  int index = this->getFileIndex(file);
  FileTextDocument *f = m_fileDocuments.removeIntermediate(index);
  xassert(f == file);
  SELF_CHECK();

  this->notifyRemoved(file);

  // Do not deallocate, we are transferring ownership to caller.
}


void FileTextDocumentList::moveFile(FileTextDocument *file, int newIndex)
{
  TRACE("file-td-list", "moveFile to " << newIndex <<
                        ": " << file->m_filename);

  int oldIndex = this->getFileIndex(file);
  xassert(oldIndex >= 0);

  m_fileDocuments.moveElement(oldIndex, newIndex);
  SELF_CHECK();

  this->notifyListOrderChanged();
}


FileTextDocument *FileTextDocumentList::createUntitledFile()
{
  FileTextDocument *file = new FileTextDocument();

  // Come up with a unique "untitled" name.
  file->m_filename = "untitled.txt";
  file->m_isUntitled = true;
  int n = 1;
  while (this->findFileByName(file->m_filename)) {
    n++;
    file->m_filename = stringb("untitled" << n << ".txt");
  }

  TRACE("file-td-list", "createUntitledFile: " << file->m_filename);
  file->m_title = file->m_filename;

  this->addFile(file);
  return file;
}


FileTextDocument *FileTextDocumentList::findFileByName(string const &filename)
{
  return const_cast<FileTextDocument*>(this->findFileByNameC(filename));
}

FileTextDocument const *FileTextDocumentList::findFileByNameC(
  string const &filename) const
{
  for (int i=0; i < m_fileDocuments.length(); i++) {
    if (m_fileDocuments[i]->m_filename == filename) {
      return m_fileDocuments[i];
    }
  }
  return NULL;
}


FileTextDocument *FileTextDocumentList::findFileByTitle(string const &title)
{
  return const_cast<FileTextDocument*>(this->findFileByTitleC(title));
}

FileTextDocument const *FileTextDocumentList::findFileByTitleC(
  string const &title) const
{
  for (int i=0; i < m_fileDocuments.length(); i++) {
    if (m_fileDocuments[i]->m_title == title) {
      return m_fileDocuments[i];
    }
  }
  return NULL;
}


FileTextDocument *FileTextDocumentList::findFileByHotkey(int hotkeyDigit)
{
  return const_cast<FileTextDocument*>(this->findFileByHotkeyC(hotkeyDigit));
}

FileTextDocument const *FileTextDocumentList::findFileByHotkeyC(
  int hotkeyDigit) const
{
  for (int i=0; i < m_fileDocuments.length(); i++) {
    if (m_fileDocuments[i]->hasHotkey() &&
        m_fileDocuments[i]->getHotkeyDigit() == hotkeyDigit) {
      return m_fileDocuments[i];
    }
  }
  return NULL;
}


FileTextDocument *FileTextDocumentList::findFileByWindowMenuId (int id)
{
  return const_cast<FileTextDocument*>(this->findFileByWindowMenuIdC(id));
}

FileTextDocument const *FileTextDocumentList::findFileByWindowMenuIdC(int id) const
{
  for (int i=0; i < m_fileDocuments.length(); i++) {
    if (m_fileDocuments[i]->m_windowMenuId == id) {
      return m_fileDocuments[i];
    }
  }
  return NULL;
}


FileTextDocument *FileTextDocumentList::findUntitledUnmodifiedFile()
{
  return const_cast<FileTextDocument*>(this->findUntitledUnmodifiedFileC());
}

FileTextDocument const *FileTextDocumentList::findUntitledUnmodifiedFileC() const
{
  for (int i=0; i < m_fileDocuments.length(); i++) {
    FileTextDocument const *file = m_fileDocuments[i];
    if (file->m_isUntitled &&
        file->numLines() == 1 &&
        file->lineLength(0) == 0) {
      TRACE("file-td-list", "findUntitledUnmodifiedFile: " << file->m_filename);
      return file;
    }
  }
  TRACE("file-td-list", "findUntitledUnmodifiedFile: NULL");
  return NULL;
}


string FileTextDocumentList::computeUniqueTitle(string filename) const
{
  TRACE("file-td-list", "computeUniqueTitle: " << filename);

  // Split the filename into path components.
  StrtokParse tok(filename, "\\/");

  // Find the minimum number of trailing components we need to
  // include to make the title unique.
  for (int n = 1; n <= tok.tokc(); n++) {
    // Construct a title with 'n' trailing components.
    stringBuilder sb;
    for (int i = tok.tokc() - n; i < tok.tokc(); i++) {
      sb << tok[i];
      if (i < tok.tokc() - 1) {
        // Make titles exclusively with forward slash.
        sb << '/';
      }
    }

    // Check for another file with this title.
    if (!this->findFileByTitleC(sb)) {
      TRACE("file-td-list", "computed title with " << n <<
                            " components: " << sb);
      return sb;
    }
  }

  // No suffix of 'filename', including itself, was unique as a title.
  // Start appending numbers.
  int n=2;
  while (n < 2000000000) {     // Prevent theoretical infinite loop.
    string s = stringb(filename << ':' << n);
    if (!this->findFileByNameC(s)) {
      TRACE("file-td-list", "computed title by appending " << n <<
                            ": " << s);
      return s;
    }
    n++;
  }

  // My tests do not cover these lines.
  TRACE("file-td-list", "failed to compute title!");
  xfailure("Could not generate a unique title string!");
  return filename;
}


void FileTextDocumentList::assignUniqueTitle(FileTextDocument *file)
{
  TRACE("file-td-list", "assignUniqueTitle: " << file->m_filename);

  // Free up the file's current title so it can remain unchanged.
  file->m_title = "";

  // Compute a new one.
  file->m_title = this->computeUniqueTitle(file->m_filename);

  this->notifyAttributeChanged(file);
  SELF_CHECK();
}


bool FileTextDocumentList::computeUniqueHotkey(int /*OUT*/ &digit) const
{
  for (int i=1; i<=10; i++) {
    // Use 0 as the tenth digit in this sequence to match the order
    // of the digit keys on the keyboard.
    digit = (i==10? 0 : i);

    if (!this->findFileByHotkeyC(digit)) {
      TRACE("file-td-list", "computeUniqueHotkey: returning " << digit);
      return true;
    }
  }

  TRACE("file-td-list", "computeUniqueHotkey: not hotkey available");
  return false;
}


void FileTextDocumentList::assignUniqueHotkey(FileTextDocument *file)
{
  TRACE("file-td-list", "assignUniqueHotkey: " << file->m_filename);

  file->clearHotkey();

  int newDigit;
  if (this->computeUniqueHotkey(newDigit)) {
    file->setHotkeyDigit(newDigit);
  }

  this->notifyAttributeChanged(file);
  SELF_CHECK();
}


void FileTextDocumentList::getUniqueDirectories(
  ArrayStack<string> /*INOUT*/ &dirs) const
{
  // Set of directories put into 'dirs' so far.
  StringSet dirSet;

  for (int i=0; i < m_fileDocuments.length(); i++) {
    if (!m_fileDocuments[i]->m_isUntitled) {
      string dir = dirname(m_fileDocuments[i]->m_filename);
      if (!dirSet.contains(dir)) {
        dirs.push(dir);
        dirSet.add(dir);
      }
    }
  }
}


void FileTextDocumentList::addObserver(FileTextDocumentListObserver *observer)
{
  TRACE("file-td-list", "addObserver: " << (void*)observer);

  xassert(!m_iteratingOverObservers);
  m_observers.appendNewItem(observer);
  SELF_CHECK();
}


void FileTextDocumentList::removeObserver(FileTextDocumentListObserver *observer)
{
  TRACE("file-td-list", "removeObserver: " << (void*)observer);

  xassert(!m_iteratingOverObservers);
  m_observers.removeItem(observer);
  SELF_CHECK();
}


void FileTextDocumentList::notifyAdded(FileTextDocument *file_)
{
  // Here, and in subsequent 'notify' routines, the idea is to hold an
  // RCSerf pointing at the file to ensure it lives throughout the
  // entire notification process.  That is, no observer is allowed to
  // deallocate 'file', either directly or indirectly.
  RCSerf<FileTextDocument> file(file_);

  TRACE("file-td-list", "notifyAdded: " << file->m_filename);

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(FileTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentAdded(this, file);
  }
}


void FileTextDocumentList::notifyRemoved(FileTextDocument *file_)
{
  RCSerf<FileTextDocument> file(file_);
  TRACE("file-td-list", "notifyRemoved: " << file->m_filename);

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(FileTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentRemoved(this, file);
  }
}


void FileTextDocumentList::notifyAttributeChanged(FileTextDocument *file_)
{
  RCSerf<FileTextDocument> file(file_);
  TRACE("file-td-list", "notifyAttributeChanged: " << file->m_filename);

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(FileTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentAttributeChanged(this, file);
  }
}


void FileTextDocumentList::notifyListOrderChanged()
{
  TRACE("file-td-list", "notifyListOrderChanged");

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(FileTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentListOrderChanged(this);
  }
}


bool FileTextDocumentList::notifyGetInitialView(
  FileTextDocument *file_,
  FileTextDocumentInitialView /*OUT*/ &view)
{
  RCSerf<FileTextDocument> file(file_);
  TRACE("file-td-list",
    stringb("notifyGetInitialView: file=" << file->m_filename));

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(FileTextDocumentListObserver, m_observers, iter) {
    if (iter.data()->getFileTextDocumentInitialView(this, file, view)) {
      TRACE("file-td-list",
        stringb("notifyGetInitialView: found: fv=" << view.firstVisible));
      return true;
    }
  }

  TRACE("file-td-list", "notifyGetInitialView: not found");
  return false;
}


// ----------------- FileTextDocumentListObserver -------------------
void FileTextDocumentListObserver::fileTextDocumentAdded(
  FileTextDocumentList *documentList, FileTextDocument *file) noexcept
{}

void FileTextDocumentListObserver::fileTextDocumentRemoved(
  FileTextDocumentList *documentList, FileTextDocument *file) noexcept
{}

void FileTextDocumentListObserver::fileTextDocumentAttributeChanged(
  FileTextDocumentList *documentList, FileTextDocument *file) noexcept
{}

void FileTextDocumentListObserver::fileTextDocumentListOrderChanged(
  FileTextDocumentList *documentList) noexcept
{}

bool FileTextDocumentListObserver::getFileTextDocumentInitialView(
  FileTextDocumentList *documentList, FileTextDocument *file,
  FileTextDocumentInitialView /*OUT*/ &view) noexcept
{
  return false;
}

FileTextDocumentListObserver::~FileTextDocumentListObserver()
{}


// EOF
