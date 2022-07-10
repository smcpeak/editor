// named-td-list.cc
// code for named-td-list.h

#include "named-td-list.h"             // this module

// smbase
#include "sm-macros.h"                 // Restorer
#include "sm-file-util.h"              // SMFileUtil
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
  this->createUntitledDocument(SMFileUtil().currentDirectory());
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

  for (int i=0; i < m_documents.length(); i++) {
    NamedTextDocument const *d = m_documents[i];
    xassert(!d->docName().isempty());
    filenames.addUnique(d->docName());
    xassert(!d->m_title.isempty());
    filenames.addUnique(d->m_title);
  }
}


int NamedTextDocumentList::numDocuments() const
{
  return m_documents.length();
}


NamedTextDocument *NamedTextDocumentList::getDocumentAt(int index)
{
  return const_cast<NamedTextDocument*>(this->getDocumentAtC(index));
}

NamedTextDocument const *NamedTextDocumentList::getDocumentAtC(int index) const
{
  return m_documents[index];
}


bool NamedTextDocumentList::hasDocument(NamedTextDocument const *file) const
{
  return getDocumentIndex(file) >= 0;
}


int NamedTextDocumentList::getDocumentIndex(NamedTextDocument const *file) const
{
  return m_documents.indexOf(file);
}


void NamedTextDocumentList::addDocument(NamedTextDocument *file)
{
  TRACE("named-td-list", "addFile: " << file->docName());
  xassert(!file->docName().empty());
  xassert(!this->hasDocument(file));

  // Assign title if necessary.
  if (file->m_title.isempty() || this->findDocumentByTitle(file->m_title)) {
    file->m_title = this->computeUniqueTitle(file->docName());
  }

  m_documents.append(file);

  this->notifyAdded(file);
  SELF_CHECK();
}


void NamedTextDocumentList::removeDocument(NamedTextDocument *file)
{
  TRACE("named-td-list", "removeFile: " << file->docName());

  if (this->numDocuments() == 1) {
    // Ensure we will not end up with an empty list.
    this->createUntitledDocument(SMFileUtil().currentDirectory());
  }

  int index = this->getDocumentIndex(file);
  NamedTextDocument *f = m_documents.removeIntermediate(index);
  xassert(f == file);
  SELF_CHECK();

  this->notifyRemoved(file);

  // Do not deallocate, we are transferring ownership to caller.
}


void NamedTextDocumentList::moveDocument(NamedTextDocument *file, int newIndex)
{
  TRACE("named-td-list", "moveFile to " << newIndex <<
                        ": " << file->docName());

  int oldIndex = this->getDocumentIndex(file);
  xassert(oldIndex >= 0);

  m_documents.moveElement(oldIndex, newIndex);
  SELF_CHECK();

  this->notifyListOrderChanged();
}


NamedTextDocument *NamedTextDocumentList::createUntitledDocument(
  string const &dir)
{
  // Come up with a unique "untitled" name.
  string name = "untitled.txt";
  int n = 1;
  while (this->findDocumentByName(name)) {
    n++;
    name = stringb("untitled" << n << ".txt");
  }

  TRACE("named-td-list", "createUntitledDocument: " << name);
  NamedTextDocument *doc = new NamedTextDocument();
  doc->setNonFileName(name, dir);
  doc->m_title = this->computeUniqueTitle(name);
  this->addDocument(doc);

  return doc;
}


NamedTextDocument *NamedTextDocumentList::findDocumentByName(string const &filename)
{
  return const_cast<NamedTextDocument*>(this->findDocumentByNameC(filename));
}

NamedTextDocument const *NamedTextDocumentList::findDocumentByNameC(
  string const &name) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->docName() == name) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findDocumentByTitle(string const &title)
{
  return const_cast<NamedTextDocument*>(this->findDocumentByTitleC(title));
}

NamedTextDocument const *NamedTextDocumentList::findDocumentByTitleC(
  string const &title) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->m_title == title) {
      return m_documents[i];
    }
  }
  return NULL;
}


NamedTextDocument *NamedTextDocumentList::findUntitledUnmodifiedDocument()
{
  return const_cast<NamedTextDocument*>(this->findUntitledUnmodifiedDocumentC());
}

NamedTextDocument const *NamedTextDocumentList::findUntitledUnmodifiedDocumentC() const
{
  for (int i=0; i < m_documents.length(); i++) {
    NamedTextDocument const *file = m_documents[i];
    if (!file->hasFilename() &&
        file->numLines() == 1 &&
        file->isEmptyLine(0)) {
      TRACE("named-td-list", "findUntitledUnmodifiedFile: " << file->docName());
      return file;
    }
  }
  TRACE("named-td-list", "findUntitledUnmodifiedFile: NULL");
  return NULL;
}


string NamedTextDocumentList::computeUniqueTitle(string filename) const
{
  TRACE("named-td-list", "computeUniqueTitle: " << filename);

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
    if (!this->findDocumentByTitleC(sb)) {
      TRACE("named-td-list", "computed title with " << n <<
                            " components: " << sb);
      return sb;
    }
  }

  // No suffix of 'filename', including itself, was unique as a title.
  // Start appending numbers.
  int n=2;
  while (n < 2000000000) {     // Prevent theoretical infinite loop.
    string s = stringb(filename << ':' << n);
    if (!this->findDocumentByNameC(s)) {
      TRACE("named-td-list", "computed title by appending " << n <<
                            ": " << s);
      return s;
    }
    n++;
  }

  // My tests do not cover these lines.
  TRACE("named-td-list", "failed to compute title!");
  xfailure("Could not generate a unique title string!");
  return filename;
}


void NamedTextDocumentList::assignUniqueTitle(NamedTextDocument *file)
{
  TRACE("named-td-list", "assignUniqueTitle: " << file->docName());

  // Free up the file's current title so it can remain unchanged.
  file->m_title = "";

  // Compute a new one.
  file->m_title = this->computeUniqueTitle(file->docName());

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
  TRACE("named-td-list", "addObserver: " << (void*)observer);

  xassert(!m_iteratingOverObservers);
  m_observers.appendNewItem(observer);
  SELF_CHECK();
}


void NamedTextDocumentList::removeObserver(NamedTextDocumentListObserver *observer)
{
  TRACE("named-td-list", "removeObserver: " << (void*)observer);

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

  TRACE("named-td-list", "notifyAdded: " << file->docName());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentAdded(this, file);
  }
}


void NamedTextDocumentList::notifyRemoved(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("named-td-list", "notifyRemoved: " << file->docName());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentRemoved(this, file);
  }
}


void NamedTextDocumentList::notifyAttributeChanged(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("named-td-list", "notifyAttributeChanged: " << file->docName());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentAttributeChanged(this, file);
  }
}


void NamedTextDocumentList::notifyListOrderChanged()
{
  TRACE("named-td-list", "notifyListOrderChanged");

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentListOrderChanged(this);
  }
}


bool NamedTextDocumentList::notifyGetInitialView(
  NamedTextDocument *file_,
  NamedTextDocumentInitialView /*OUT*/ &view)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("named-td-list",
    stringb("notifyGetInitialView: file=" << file->docName()));

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    if (iter.data()->getNamedTextDocumentInitialView(this, file, view)) {
      TRACE("named-td-list",
        stringb("notifyGetInitialView: found: fv=" << view.firstVisible));
      return true;
    }
  }

  TRACE("named-td-list", "notifyGetInitialView: not found");
  return false;
}


// ----------------- NamedTextDocumentListObserver -------------------
void NamedTextDocumentListObserver::namedTextDocumentAdded(
  NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT
{}

void NamedTextDocumentListObserver::namedTextDocumentRemoved(
  NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT
{}

void NamedTextDocumentListObserver::namedTextDocumentAttributeChanged(
  NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT
{}

void NamedTextDocumentListObserver::namedTextDocumentListOrderChanged(
  NamedTextDocumentList *documentList) NOEXCEPT
{}

bool NamedTextDocumentListObserver::getNamedTextDocumentInitialView(
  NamedTextDocumentList *documentList, NamedTextDocument *file,
  NamedTextDocumentInitialView /*OUT*/ &view) NOEXCEPT
{
  return false;
}

NamedTextDocumentListObserver::~NamedTextDocumentListObserver()
{}


// EOF
