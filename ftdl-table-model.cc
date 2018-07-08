// ftdl-table-model.cc
// code for ftdl-table-model.h

#include "ftdl-table-model.h"          // this module

// editor
#include "dev-warning.h"               // DEV_WARNING

// smqtutil
#include "qtutil.h"                    // toQString(string)

// smbase
#include "trace.h"                     // TRACE


// Tracing, but only when I enable it manually, since these methods
// are called many times.  Even when disabled, I still verify that it
// compiles so that the messages do not bitrot.
bool const ENABLE_TRACE_THIS = false;

#define TRACE_THIS(msg)             \
  if (ENABLE_TRACE_THIS) {          \
    TRACE("FTDLTableModel", msg);   \
  }                                 \
  else                              \
    ((void)0) /* user ; */


int FTDLTableModel::s_objectCount = 0;


FTDLTableModel::FTDLTableModel(FileTextDocumentList *docList,
                               QObject *parent)
  : QAbstractTableModel(parent),
    m_docList(docList)
{
  s_objectCount++;
}


FTDLTableModel::~FTDLTableModel()
{
  s_objectCount--;
}


/*static*/ char const *FTDLTableModel::columnName(TableColumn tc)
{
  static char const * const names[] = {
    "File name",
    "Lines",
  };
  ASSERT_TABLESIZE(names, NUM_TABLE_COLUMNS);
  xassert((unsigned)tc < NUM_TABLE_COLUMNS);
  return names[tc];
}


// For use with TRACE.
static ostream& operator<< (ostream &os, QModelIndex const &index)
{
  if (!index.isValid()) {
    return os << "root";
  }
  else {
    return os << index.parent()
              << ".(r=" << index.row()
              << ", c=" << index.column() << ')';
  }
}



int FTDLTableModel::rowCount(QModelIndex const &parent) const
{
  TRACE_THIS("rowCount(" << parent << ')');

  if (parent.isValid()) {
    // Qt docs: "When implementing a table based model, rowCount()
    // should return 0 when the parent is valid."
    return 0;
  }

  return m_docList->numFiles();
}


int FTDLTableModel::columnCount(QModelIndex const &parent) const
{
  TRACE_THIS("columnCount(" << parent << ')');

  if (parent.isValid()) {
    return 0;
  }

  return NUM_TABLE_COLUMNS;
}


QVariant FTDLTableModel::data(QModelIndex const &index, int role) const
{
  TRACE_THIS("data(" << index << ", " << role << ')');

  if (index.parent().isValid()) {
    // This is a table with no sub-tables, so return nothing.
    return QVariant();
  }

  int r = index.row();
  if (!( 0 <= r && r < m_docList->numFiles() )) {
    DEV_WARNING("invalid row: r=" << r << " nf=" <<
                m_docList->numFiles());
    return QVariant();
  }
  FileTextDocument *doc = m_docList->getFileAt(r);

  int c = index.column();
  if (!( 0 <= c && c < NUM_TABLE_COLUMNS )) {
    DEV_WARNING("invalid column: " << c);
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    switch (c) {
      case TC_FILENAME: {
        stringBuilder sb;
        sb << doc->filename;
        if (doc->unsavedChanges()) {
          sb << " *";
        }
        return toQString(sb);
      }

      case TC_LINES:
        return qstringb(doc->numLines());

      case NUM_TABLE_COLUMNS:
        DEV_WARNING("impossible!");
        return QVariant();

      // I do not have a 'default' because I want to get a compiler
      // warning if I leave out a case.  But I'm not getting one in my
      // quick testing so I don't know if this matters.
    }
  }

  if (role == Qt::TextAlignmentRole && c == TC_LINES) {
    // The line counts are integers, which should be right-aligned.
    //
    // I do not understand why I need this "operator Int()".  The
    // conversion to QVariant should work without it, but GCC is
    // unhappy.
    return (Qt::AlignRight | Qt::AlignVCenter).operator Int();
  }

  // Some other role.
  return QVariant();
}


QVariant FTDLTableModel::headerData(int section,
  Qt::Orientation orientation, int role) const
{
  TRACE_THIS("headerData(" << section << ", " <<
             orientation << ", " << role << ')');

  if (orientation == Qt::Horizontal) {
    int c = section;
    if (!( 0 <= c && c < NUM_TABLE_COLUMNS )) {
      DEV_WARNING("invalid column: " << c);
      return QVariant();
    }

    if (role == Qt::DisplayRole) {
      return QString(columnName((TableColumn)c));
    }
    else if (role == Qt::TextAlignmentRole && c == TC_LINES) {
      return (Qt::AlignRight | Qt::AlignVCenter).operator Int();
    }
    else {
      return QVariant();
    }
  }

  else if (orientation == Qt::Vertical) {
    // No row headers.
    return QVariant();
  }

  else {
    DEV_WARNING("invalid orientation: " << orientation);
    return QVariant();
  }
}


// EOF
