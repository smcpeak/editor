// named-td-list.cc
// code for named-td-list.h

#include "named-td-list.h"             // this module

// smbase
#include "container-utils.h"           // insertUnique
#include "sm-macros.h"                 // Restorer
#include "sm-file-util.h"              // SMFileUtil
#include "stringset.h"                 // StringSet
#include "strtokp.h"                   // StrtokParse
#include "strutil.h"                   // dirname
#include "trace.h"                     // TRACE

// libc++
#include <set>                         // std::set


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


void NamedTextDocumentList::selfCheck() const
{
  xassert(m_documents.isNotEmpty());

  // Sets of attributes seen, to check for uniqueness.
  std::set<DocumentName> docNames;
  StringSet titles;

  for (int i=0; i < m_documents.length(); i++) {
    NamedTextDocument const *d = m_documents[i];

    xassert(!d->documentName().empty());
    insertUnique(docNames, d->documentName());

    xassert(!d->m_title.isempty());
    titles.addUnique(d->m_title);
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
  TRACE("named-td-list", "addFile: " << file->documentName());
  xassert(!file->documentName().empty());
  xassert(!this->hasDocument(file));

  // Assign title if necessary.
  if (file->m_title.isempty() || this->findDocumentByTitle(file->m_title)) {
    file->m_title = this->computeUniqueTitle(file->documentName());
  }

  m_documents.append(file);

  this->notifyAdded(file);
  SELF_CHECK();
}


void NamedTextDocumentList::removeDocument(NamedTextDocument *file)
{
  TRACE("named-td-list", "removeFile: " << file->documentName());

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
                        ": " << file->documentName());

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
  DocumentName docName;
  HostName hostName(HostName::asLocal());
  docName.setNonFileResourceName(hostName, "untitled.txt", dir);
  int n = 1;
  while (this->findDocumentByName(docName)) {
    n++;
    docName.setNonFileResourceName(hostName,
      stringb("untitled" << n << ".txt"), dir);
    xassert(n < 1000);       // Prevent infinite loop.
  }

  TRACE("named-td-list", "createUntitledDocument: " << docName);
  NamedTextDocument *doc = new NamedTextDocument();
  doc->setDocumentName(docName);
  doc->m_title = this->computeUniqueTitle(docName);
  this->addDocument(doc);

  return doc;
}


NamedTextDocument *NamedTextDocumentList::findDocumentByName(
  DocumentName const &docName)
{
  return const_cast<NamedTextDocument*>(this->findDocumentByNameC(docName));
}

NamedTextDocument const *NamedTextDocumentList::findDocumentByNameC(
  DocumentName const &name) const
{
  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->documentName() == name) {
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
      TRACE("named-td-list", "findUntitledUnmodifiedFile: " << file->documentName());
      return file;
    }
  }
  TRACE("named-td-list", "findUntitledUnmodifiedFile: NULL");
  return NULL;
}


string NamedTextDocumentList::computeUniqueTitle(
  DocumentName const &docName) const
{
  TRACE("named-td-list", "computeUniqueTitle: " << docName);

  // Split the filename into path components.
  StrtokParse tok(docName.resourceName(), "\\/");

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
  //
  // This never happens in practice, but it is exercised by the unit
  // tests for this module.
  int n=2;
  while (n < 2000000000) {     // Prevent theoretical infinite loop.
    string s = stringb(docName.resourceName() << ':' << n);
    if (!this->findDocumentByTitleC(s)) {
      TRACE("named-td-list", "computed title by appending " << n <<
                            ": " << s);
      return s;
    }
    n++;
  }

  // My tests do not cover these lines.
  TRACE("named-td-list", "failed to compute title!");
  xfailure("Could not generate a unique title string!");
  return "";
}


void NamedTextDocumentList::assignUniqueTitle(NamedTextDocument *file)
{
  TRACE("named-td-list", "assignUniqueTitle: " << file->documentName());

  // Free up the file's current title so it can remain unchanged.
  file->m_title = "";

  // Compute a new one.
  file->m_title = this->computeUniqueTitle(file->documentName());

  this->notifyAttributeChanged(file);
  SELF_CHECK();
}


void NamedTextDocumentList::getUniqueDirectories(
  ArrayStack<HostAndResourceName> /*INOUT*/ &dirs) const
{
  // Set of directories put into 'dirs' so far.
  std::set<HostAndResourceName> dirSet;

  // Add the existing entries so we do not duplicate them.
  for (int i=0; i < dirs.length(); i++) {
    HostAndResourceName const &dir = dirs[i];
    dirSet.insert(dir);
    TRACE("named-td-list", "getUniqueDirectories: "
      "dirs already contains: " << dir);
  }

  for (int i=0; i < m_documents.length(); i++) {
    if (m_documents[i]->hasFilename()) {
      HostAndResourceName dir = m_documents[i]->directoryHarn();
      if (!contains(dirSet, dir)) {
        dirs.push(dir);
        dirSet.insert(dir);

        TRACE("named-td-list", "getUniqueDirectories: "
          "adding " << dir <<
          " due to " << m_documents[i]->harn());
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

  TRACE("named-td-list", "notifyAdded: " << file->documentName());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentAdded(this, file);
  }
}


void NamedTextDocumentList::notifyRemoved(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("named-td-list", "notifyRemoved: " << file->documentName());

  Restorer<bool> restorer(m_iteratingOverObservers, true);
  FOREACH_RCSERFLIST_NC(NamedTextDocumentListObserver, m_observers, iter) {
    iter.data()->namedTextDocumentRemoved(this, file);
  }
}


void NamedTextDocumentList::notifyAttributeChanged(NamedTextDocument *file_)
{
  RCSerf<NamedTextDocument> file(file_);
  TRACE("named-td-list", "notifyAttributeChanged: " << file->documentName());

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
    stringb("notifyGetInitialView: file=" << file->documentName()));

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
