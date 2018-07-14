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


// --------------------- NamedTextDocumentList -----------------------
NamedTextDocumentList::NamedTextDocumentList()
  : m_observers(),
    m_iteratingOverObservers(false),
    m_documents()
{
  this->createUntitledFile();
  SELF_CHECK();
}


NamedTextDocumentList::~NamedTextDocumentList()
{
  // Do this explicitly for clarity.
  m_observers.removeAll();
  m_documents.deleteAll();
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


void NamedTextDocumentList::selfCheck() const
{
  xassert(m_documents.isNotEmpty());

  // Sets of attributes seen, to check for uniqueness.
  StringSet filenames;
  StringSet titles;
  set<int> hotkeyDigits;
  set<int> windowMenuIds;

  for (int i=0; i < m_documents.length(); i++) {
    NamedTextDocument const *d = m_documents[i];
    xassert(!d->name().isempty());
    filenames.addUnique(d->name());
    xassert(!d->m_title.isempty());
    filenames.addUnique(d->m_title);
    if (d->hasHotkey()) {
      insertUnique(hotkeyDigits, d->getHotkeyDigit());
    }
    insertUnique(windowMenuIds, d->m_windowMenuId);
  }
}


int NamedTextDocumentList::numFiles() const
{
  return m_documents.length();
}


NamedTextDocument *NamedTextDocumentList::getFileAt(int index)
{
  return const_cast<NamedTextDocument*>(this->getFileAtC(index));
}

NamedTextDocument const *NamedTextDocumentList::getFileAtC(int index) const
{
  return m_documents[index];
}


bool NamedTextDocumentList::hasFile(NamedTextDocument const *file) const
{
  return getFileIndex(file) >= 0;
}


int NamedTextDocumentList::getFileIndex(NamedTextDocument const *file) const
{
  return m_documents.indexOf(file);
}


void NamedTextDocumentList::addFile(NamedTextDocument *file)
{
  TRACE("file-td-list", "addFile: " << file->name());
  xassert(!this->hasFile(file));

  // Assign title if necessary.
  if (file->m_title.isempty() || this->findFileByTitle(file->m_title)) {
    file->m_title = this->computeUniqueTitle(file->name());
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

  m_documents.append(file);

  this->notifyAdded(file);
  SELF_CHECK();
}


void NamedTextDocumentList::removeFile(NamedTextDocument *file)
{
  TRACE("file-td-list", "removeFile: " << file->name());

  // If we make an untitled file, allow it to take the same hotkey.
  file->clearHotkey();

  if (this->numFiles() == 1) {
    // Ensure we will not end up with an empty list.
    this->createUntitledFile();
  }

  int index = this->getFileIndex(file);
  NamedTextDocument *f = m_documents.removeIntermediate(index);
  xassert(f == file);
  SELF_CHECK();

  this->notifyRemoved(file);

  // Do not deallocate, we are transferring ownership to caller.
}


void NamedTextDocumentList::moveFile(NamedTextDocument *file, int newIndex)
{
  TRACE("file-td-list", "moveFile to " << newIndex <<
                        ": " << file->name());

  int oldIndex = this->getFileIndex(file);
  xassert(oldIndex >= 0);

  m_documents.moveElement(oldIndex, newIndex);
  SELF_CHECK();

  this->notifyListOrderChanged();
}


NamedTextDocument *NamedTextDocumentList::createUntitledFile()
{
  // TODO: Rewrite this code slightly to delay creating the document
  // object until the name has been computed, just to be a bit cleaner.
  NamedTextDocument *file = new NamedTextDocument();

  // Come up with a unique "untitled" name.
  file->setNonFileName("untitled.txt");
  int n = 1;
  while (this->findFileByName(file->name())) {
    n++;
    file->setNonFileName(stringb("untitled" << n << ".txt"));
  }

  TRACE("file-td-list", "createUntitledFile: " << file->name());
  file->m_title = file->name();

  this->addFile(file);
  return file;
}


NamedTextDocument *NamedTextDocumentList::findFileByName(string const &filename)
{
  return const_cast<NamedTextDocument*>(this->findFileByNameC(filename));
}

NamedTextDocument const *NamedTextDocumentList::findFileByNameC(
  string const &name) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->name() == name) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findFileByTitle(string const &title)
{
  return const_cast<NamedTextDocument*>(this->findFileByTitleC(title));
}

NamedTextDocument const *NamedTextDocumentList::findFileByTitleC(
  string const &title) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->m_title == title) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findFileByHotkey(int hotkeyDigit)
{
  return const_cast<NamedTextDocument*>(this->findFileByHotkeyC(hotkeyDigit));
}

NamedTextDocument const *NamedTextDocumentList::findFileByHotkeyC(
  int hotkeyDigit) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->hasHotkey() &&
        m_documents[i]->getHotkeyDigit() == hotkeyDigit) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findFileByWindowMenuId (int id)
{
  return const_cast<NamedTextDocument*>(this->findFileByWindowMenuIdC(id));
}

NamedTextDocument const *NamedTextDocumentList::findFileByWindowMenuIdC(int id) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->m_windowMenuId == id) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findUntitledUnmodifiedFile()
{
  return const_cast<NamedTextDocument*>(this->findUntitledUnmodifiedFileC());
}

NamedTextDocument const *NamedTextDocumentList::findUntitledUnmodifiedFileC() const
{
  for (int i=0; i < m_documents.length(); i++) {
    NamedTextDocument const *file = m_documents[i];
    if (!file->hasFilename() &&
        file->numLines() == 1 &&
        file->lineLength(0) == 0) {
      TRACE("file-td-list", "findUntitledUnmodifiedFile: " << file->name());
      return file;
    }
  }
  TRACE("file-td-list", "findUntitledUnmodifiedFile: NULL");
  return NULL;
}


string NamedTextDocumentList::computeUniqueTitle(string filename) const
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


void NamedTextDocumentList::assignUniqueTitle(NamedTextDocument *file)
{
  TRACE("file-td-list", "assignUniqueTitle: " << file->name());

  // Free up the file's current title so it can remain unchanged.
  file->m_title = "";

  // Compute a new one.
  file->m_title = this->computeUniqueTitle(file->name());

  this->notifyAttributeChanged(file);
  SELF_CHECK();
}


bool NamedTextDocumentList::computeUniqueHotkey(int /*OUT*/ &digit) const
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


void NamedTextDocumentList::assignUniqueHotkey(NamedTextDocument *file)
{
  TRACE("file-td-list", "assignUniqueHotkey: " << file->name());

  file->clearHotkey();

  int newDigit;
  if (this->computeUniqueHotkey(newDigit)) {
    file->setHotkeyDigit(newDigit);
  }

  this->notifyAttributeChanged(file);
  SELF_CHECK();
}


void NamedTextDocumentList::getUniqueDirectories(
  ArrayStack<string> /*INOUT*/ &dirs) const
{
  // Set of directories put into 'dirs' so far.
  StringSet dirSet;

  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->hasFilename()) {
      string dir = dirname(m_documents[i]->filename());
      if (!dirSet.contains(dir)) {
        dirs.push(dir);
        dirSet.add(dir);
      }
    }
  }
}


void NamedTextDocumentList::addObserver(NamedTextDocumentListObserver *observer)
{
  TRACE("file-td-list", "addObserver: " << (void*)observer);

  xassert(!m_iteratingOverObservers);
  m_observers.appendNewItem(observer);
  SELF_CHECK();
}


void NamedTextDocumentList::removeObserver(NamedTextDocumentListObserver *observer)
{
  TRACE("file-td-list", "removeObserver: " << (void*)observer);

  xassert(!m_iteratingOverObservers);
  m_observers.removeItem(observer);
  SELF_CHECK();
}


void NamedTextDocumentList::notifyAdded(NamedTextDocument *file_)
{
  // Here, and in subsequent 'notify' routines, the idea is to hold an
  // RCSerf pointing at the file to ensure it lives throughout the
  // entire notification process.  That is, no observer is allowed to
  // deallocate 'file', either directly or indirectly.
  RCSerf<NamedTextDocument> file(file_);

  TRACE("file-td-list", "notifyAdded: " << file->name());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentAdded(this, file);
  }
}


void NamedTextDocumentList::notifyRemoved(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("file-td-list", "notifyRemoved: " << file->name());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentRemoved(this, file);
  }
}


void NamedTextDocumentList::notifyAttributeChanged(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("file-td-list", "notifyAttributeChanged: " << file->name());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentAttributeChanged(this, file);
  }
}


void NamedTextDocumentList::notifyListOrderChanged()
{
  TRACE("file-td-list", "notifyListOrderChanged");

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->fileTextDocumentListOrderChanged(this);
  }
}


bool NamedTextDocumentList::notifyGetInitialView(
  NamedTextDocument *file_,
  NamedTextDocumentInitialView /*OUT*/ &view)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("file-td-list",
    stringb("notifyGetInitialView: file=" << file->name()));

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    if (iter.data()->getFileTextDocumentInitialView(this, file, view)) {
      TRACE("file-td-list",
        stringb("notifyGetInitialView: found: fv=" << view.firstVisible));
      return true;
    }
  }

  TRACE("file-td-list", "notifyGetInitialView: not found");
  return false;
}


// ----------------- NamedTextDocumentListObserver -------------------
void NamedTextDocumentListObserver::fileTextDocumentAdded(
  NamedTextDocumentList *documentList, NamedTextDocument *file) noexcept
{}

void NamedTextDocumentListObserver::fileTextDocumentRemoved(
  NamedTextDocumentList *documentList, NamedTextDocument *file) noexcept
{}

void NamedTextDocumentListObserver::fileTextDocumentAttributeChanged(
  NamedTextDocumentList *documentList, NamedTextDocument *file) noexcept
{}

void NamedTextDocumentListObserver::fileTextDocumentListOrderChanged(
  NamedTextDocumentList *documentList) noexcept
{}

bool NamedTextDocumentListObserver::getFileTextDocumentInitialView(
  NamedTextDocumentList *documentList, NamedTextDocument *file,
  NamedTextDocumentInitialView /*OUT*/ &view) noexcept
{
  return false;
}

NamedTextDocumentListObserver::~NamedTextDocumentListObserver()
{}


// EOF
