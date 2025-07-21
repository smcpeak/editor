// editor-widget.cc
// code for editor-widget.h

#include "editor-widget.h"                       // this module

// editor
#include "debug-values.h"                        // DEBUG_VALUES
#include "editor-command.ast.gen.h"              // EditorCommand
#include "editor-global.h"                       // EditorGlobal
#include "editor-window.h"                       // EditorWindow
#include "nearby-file.h"                         // getNearbyFilename
#include "styledb.h"                             // StyleDB, TextCategoryAndStyle
#include "textcategory.h"                        // LineCategories, etc.
#include "vfs-query-sync.h"                      // VFS_QuerySync

// smqtutil
#include "smqtutil/courB24_ISO8859_1.bdf.gen.h"  // bdfFontData_courB24_ISO8859_1
#include "smqtutil/courO24_ISO8859_1.bdf.gen.h"  // bdfFontData_courO24_ISO8859_1
#include "smqtutil/courR24_ISO8859_1.bdf.gen.h"  // bdfFontData_courR24_ISO8859_1
#include "smqtutil/editor14b.bdf.gen.h"          // bdfFontData_editor14b
#include "smqtutil/editor14i.bdf.gen.h"          // bdfFontData_editor14i
#include "smqtutil/editor14r.bdf.gen.h"          // bdfFontData_editor14r
#include "smqtutil/minihex6.bdf.gen.h"           // bdfFontData_minihex6
#include "smqtutil/qtbdffont.h"                  // QtBDFFont
#include "smqtutil/qtguiutil.h"                  // keysString(QKeyEvent)
#include "smqtutil/qtutil.h"                     // toString(QString)
#include "smqtutil/qtutil.h"                     // SET_QOBJECT_NAME

// smbase
#include "smbase/array.h"                        // Array
#include "smbase/bdffont.h"                      // BDFFont
#include "smbase/dev-warning.h"                  // DEV_WARNING
#include "smbase/exc.h"                          // GENERIC_CATCH_BEGIN/END, smbase::{XBase, XMessage, xmessage}
#include "smbase/gdvalue.h"                      // gdv::toGDValue
#include "smbase/save-restore.h"                 // SetRestore
#include "smbase/nonport.h"                      // getMilliseconds
#include "smbase/objcount.h"                     // CHECK_OBJECT_COUNT
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/strutil.h"                      // dirname
#include "smbase/trace.h"                        // TRACE
#include "smbase/xassert.h"                      // xassert

// Qt
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>

// libc++
#include <algorithm>                             // std::min
#include <utility>                               // std::move

using namespace gdv;
using namespace smbase;


// The basic rule for using this is it should be present in any function
// that calls a non-const method of TextDocumentEditor.  This includes
// cursor changes, even though those currently do not have an associated
// notification event, since they might have one in the future.  In
// order to not have redundant code, I mostly only use this in those
// functions that make direct calls.  For consistency, I even do this
// for the destructor, or when I know I am not listening, since there is
// essentially no cost to doing it.
#define INITIATING_DOCUMENT_CHANGE()                       \
  SetRestore<bool> ignoreNotificationsRestorer(              \
    m_ignoreTextDocumentNotifications, true) /* user ; */


// Invoke `command`, passing a `unique_ptr` to a newly created command
// object of type `CommandType`, and forwarding arguments as needed.
#define COMMAND_MU(CommandType, ...) \
  command(std::make_unique<CommandType>(__VA_ARGS__));


// Distance below the baseline to draw an underline.
int const UNDERLINE_OFFSET = 2;

// Number of columns to move for Ctrl+Shift+<arrow>.
int const CTRL_SHIFT_DISTANCE = 10;

// Desired line/column gap between search match and screen edge.
int const SAR_SCROLL_GAP = 10;


// ---------------------- EditorWidget --------------------------------
int EditorWidget::s_objectCount = 0;

bool EditorWidget::s_ignoreTextDocumentNotificationsGlobally = false;

CHECK_OBJECT_COUNT(EditorWidget);


EditorWidget::EditorWidget(NamedTextDocument *tdf,
                           NamedTextDocumentList *documentList,
                           EditorWindow *editorWindow)
  : QWidget(editorWindow),
    m_editorWindow(editorWindow),
    m_infoBox(NULL),
    m_matchesAboveLabel(NULL),
    m_matchesBelowLabel(NULL),
    m_editorList(),
    m_editor(NULL),
    m_documentList(documentList),
    m_fileStatusRequestID(0),
    m_fileStatusRequestEditor(),
    m_hitText(""),
    m_hitTextFlags(TextSearch::SS_CASE_INSENSITIVE),
    m_textSearch(NULL),
    m_topMargin(1),
    m_leftMargin(1),
    m_interLineSpace(0),
    m_cursorColor(0xFF, 0xFF, 0xFF),       // white
    m_fontForCategory(NUM_STANDARD_TEXT_CATEGORIES),
    m_cursorFontForFV(FV_BOLD + 1),
    m_minihexFont(),
    m_visibleWhitespace(true),
    m_whitespaceOpacity(32),
    m_trailingWhitespaceBgColor(255, 0, 0, 64),
    m_softMarginColumn(72),
    m_visibleSoftMargin(true),
    m_softMarginColor(0xFF, 0xFF, 0xFF, 32),
    // font metrics inited by setFont()
    m_listening(false),
    m_ignoreTextDocumentNotifications(false),
    m_ignoreScrollSignals(false)
{
  xassert(tdf);

  #define MAKE_HIDDEN_LABEL(name) \
    name = new QLabel(this);      \
    SET_QOBJECT_NAME(name);       \
    name->setVisible(false)

  MAKE_HIDDEN_LABEL(m_matchesAboveLabel);
  MAKE_HIDDEN_LABEL(m_matchesBelowLabel);

  #undef MAKE_HIDDEN_LABEL

  // This will always make a new editor object since m_editorList is
  // empty, but it also adds it to m_editorList and may initialize the
  // view from another window.
  m_editor = this->getOrMakeEditor(tdf);
  this->startListening();

  m_textSearch = new TextSearch(m_editor->getDocumentCore());
  this->setTextSearchParameters();

  m_documentList->addObserver(this);

  setFontsFromEditorGlobal();

  setCursor(Qt::IBeamCursor);

  // required to accept focus
  setFocusPolicy(Qt::StrongFocus);

  // This causes 'this->eventFilter()' to be invoked when 'this'
  // receives events.  It is needed to ensure Tab gets seen by the
  // editor widget.
  installEventFilter(this);

  QObject::connect(vfsConnections(), &VFS_Connections::signal_replyAvailable,
                   this, &EditorWidget::on_replyAvailable);
  QObject::connect(vfsConnections(), &VFS_Connections::signal_failed,
                   this, &EditorWidget::on_connectionFailed);

  EditorWidget::s_objectCount++;
}


EditorWidget::~EditorWidget()
{
  EditorWidget::s_objectCount--;

  this->stopListening();

  m_documentList->removeObserver(this);
  m_documentList = NULL;

  m_textSearch.del();

  QObject::disconnect(vfsConnections(), nullptr, this, nullptr);
  cancelFileStatusRequestIfAny();

  // Do this explicitly just for clarity, but the automatic destruction
  // should also work.
  m_editor = NULL;
  m_editorList.deleteAll();

  m_fontForCategory.deleteAll();
}


void EditorWidget::selfCheck() const
{
  // Check that 'editor' is among 'm_editors' and that the files in
  // 'm_editors' are a subset of 'm_documentList'.
  bool foundEditor = false;
  FOREACH_OBJLIST(NamedTextDocumentEditor, m_editorList, iter) {
    NamedTextDocumentEditor const *tdfe = iter.data();
    if (m_editor == tdfe) {
      foundEditor = true;
    }
    tdfe->selfCheck();
    xassert(m_documentList->hasDocument(tdfe->m_namedDoc));
  }
  xassert(foundEditor);

  // There should never be more m_editors than fileDocuments.
  xassert(m_documentList->numDocuments() >= m_editorList.count());

  xassert((m_fileStatusRequestID == 0) ==
          (m_fileStatusRequestEditor == nullptr));
  xassert(m_fileStatusRequestEditor == nullptr ||
          m_fileStatusRequestEditor == m_editor);

  // Check that 'm_listening' agrees with the document's observer list.
  xassert(m_listening == m_editor->hasObserver(this));

  // And, at this point, we should always be listening.
  xassert(m_listening);

  // Check 'm_textSearch'.
  xassert(m_hitText == m_textSearch->searchString());
  xassert(m_hitTextFlags == m_textSearch->searchStringFlags());
}


void EditorWidget::setReadOnly(bool readOnly)
{
  m_editor->setReadOnly(readOnly);

  // Emit the viewChanged signal so the "read-only" indicator in the
  // File menu will be refreshed.
  redraw();
}


EditorWindow *EditorWidget::editorWindow() const
{
  return m_editorWindow;
}


EditorGlobal *EditorWidget::editorGlobal() const
{
  return editorWindow()->editorGlobal();
}


EditorSettings const &EditorWidget::editorSettings() const
{
  return editorGlobal()->getSettings();
}


void EditorWidget::cursorTo(TextLCoord tc)
{
  INITIATING_DOCUMENT_CHANGE();
  m_editor->setCursor(tc);
}


static BDFFont *makeBDFFont(char const *bdfData, char const *context)
{
  try {
    BDFFont *ret = new BDFFont;
    parseBDFString(*ret, bdfData);
    return ret;
  }
  catch (XBase &x) {
    x.prependContext(context);
    throw;
  }
}


void EditorWidget::setFontsFromEditorGlobal()
{
  if (editorGlobal()->getEditorBuiltinFont() == BF_COURIER24) {
    setFonts(bdfFontData_courR24_ISO8859_1,
             bdfFontData_courO24_ISO8859_1,
             bdfFontData_courB24_ISO8859_1);
  }
  else {
    setFonts(bdfFontData_editor14r,
             bdfFontData_editor14i,
             bdfFontData_editor14b);
  }
}


void EditorWidget::setFonts(char const *normal, char const *italic, char const *bold)
{
  // Read the font files, and index the results by FontVariant.
  ObjArrayStack<BDFFont> bdfFonts(3);
  STATIC_ASSERT(FV_NORMAL == 0);
  bdfFonts.push(makeBDFFont(normal, "normal font"));
  STATIC_ASSERT(FV_ITALIC == 1);
  bdfFonts.push(makeBDFFont(italic, "italic font"));
  STATIC_ASSERT(FV_BOLD == 2);
  bdfFonts.push(makeBDFFont(bold, "bold font"));

  // Using one fixed global style mapping.
  StyleDB *styleDB = StyleDB::instance();

  // Build the complete set of new fonts.
  {
    ObjArrayStack<QtBDFFont> newFonts(NUM_STANDARD_TEXT_CATEGORIES);
    for (int category = TC_ZERO; category < NUM_STANDARD_TEXT_CATEGORIES; category++) {
      TextStyle const &ts = styleDB->getStyle((TextCategory)category);

      STATIC_ASSERT(FV_BOLD == 2);
      BDFFont *bdfFont = bdfFonts[ts.variant % 3];

      QtBDFFont *qfont = new QtBDFFont(*bdfFont);
      qfont->setFgColor(ts.foreground);
      qfont->setBgColor(ts.background);
      qfont->setTransparent(false);
      newFonts.push(qfont);
    }

    // Substitute the new for the old.
    m_fontForCategory.swapWith(newFonts);
  }

  // Repeat the procedure for the cursor fonts.
  {
    ObjArrayStack<QtBDFFont> newFonts(FV_BOLD + 1);
    for (int fv = 0; fv <= FV_BOLD; fv++) {
      QtBDFFont *qfont = new QtBDFFont(*(bdfFonts[fv]));

      // The character under the cursor is drawn with the normal background
      // color, and the cursor box (its background) is drawn in 'cursorColor'.
      qfont->setFgColor(styleDB->getStyle(TC_NORMAL).background);
      qfont->setBgColor(m_cursorColor);
      qfont->setTransparent(false);

      newFonts.push(qfont);
    }

    m_cursorFontForFV.swapWith(newFonts);
  }

  // calculate metrics
  QRect const &bbox = m_fontForCategory[TC_NORMAL]->getAllCharsBBox();
  m_fontAscent = -bbox.top();
  m_fontDescent = bbox.bottom() + 1;
  m_fontHeight = m_fontAscent + m_fontDescent;
  xassert(m_fontHeight == bbox.height());    // check my assumptions
  m_fontWidth = bbox.width();

  // Font for missing glyphs.
  Owner<BDFFont> minihexBDFFont(makeBDFFont(bdfFontData_minihex6, "minihex font"));
  m_minihexFont = new QtBDFFont(*minihexBDFFont);
  m_minihexFont->setTransparent(false);
}


void EditorWidget::setDocumentFile(NamedTextDocument *file)
{
  this->stopListening();
  this->cancelFileStatusRequestIfAny();

  m_editor = this->getOrMakeEditor(file);

  // This deallocates the old 'TextSearch'.
  m_textSearch = new TextSearch(m_editor->getDocumentCore());
  this->setTextSearchParameters();

  // Move the chosen file to the top of the document list since it is
  // now the most recently used.
  m_documentList->moveDocument(file, 0);

  this->startListening();

  // Draw the current contents.
  this->redraw();

  // Then, issue a request to refresh those contents.
  this->requestFileStatus();
}


EditorWidget::NamedTextDocumentEditor *
  EditorWidget::getOrMakeEditor(NamedTextDocument *file_)
{
  // Hold 'file' in an RCSerf to ensure it does not go away.  In
  // particular, this method calls a 'notify' routine, which could
  // conceivably invoke code all over the place.
  RCSerf<NamedTextDocument> file(file_);

  // Look for an existing editor for this file.
  FOREACH_OBJLIST_NC(NamedTextDocumentEditor, m_editorList, iter) {
    if (iter.data()->m_namedDoc == file) {
      return iter.data();
    }
  }

  // Have to make a new editor.
  //
  // Lets ask the other windows if they know a good starting position.
  // This allows a user to open a new window without losing their
  // position in all of the open files.
  NamedTextDocumentInitialView view;
  bool hasView = m_documentList->notifyGetInitialView(file, view);

  // Make the new editor.
  NamedTextDocumentEditor *ret = new NamedTextDocumentEditor(file);
  m_editorList.prepend(ret);

  // Possibly set the initial location.
  if (hasView) {
    INITIATING_DOCUMENT_CHANGE();
    ret->setFirstVisible(view.firstVisible);
    ret->setCursor(view.cursor);

    // We do not scroll to cursor here.  If the cursor is offscreen,
    // scrolling will happen on the first keypress.  Furthermore,
    // during window creation, this function is called before the
    // true window size is known.
  }

  return ret;
}


void EditorWidget::requestFileStatus()
{
  if (!hasValidFileAndHostName()) {
    return;
  }
  if (getDocument()->m_modifiedOnDisk) {
    // We already know it has been modified.
    return;
  }

  cancelFileStatusRequestIfAny();

  std::unique_ptr<VFS_FileStatusRequest> req(new VFS_FileStatusRequest);
  req->m_path = getDocument()->filename();
  vfsConnections()->issueRequest(m_fileStatusRequestID,
    getDocument()->hostName(), std::move(req));
  m_fileStatusRequestEditor = m_editor;

  TRACE("EditorWidget",
    "requestFileStatus: request id=" << m_fileStatusRequestID);
}


void EditorWidget::cancelFileStatusRequestIfAny()
{
  if (m_fileStatusRequestID) {
    TRACE("EditorWidget", "cancelRequest: id=" << m_fileStatusRequestID);
    vfsConnections()->cancelRequest(m_fileStatusRequestID);
    m_fileStatusRequestID = 0;
    m_fileStatusRequestEditor = nullptr;
  }
}


void EditorWidget::on_replyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (requestID == m_fileStatusRequestID) {
    TRACE("EditorWidget", "on_replyAvailable: id=" << requestID);

    xassert(m_editor == m_fileStatusRequestEditor);

    m_fileStatusRequestID = 0;
    m_fileStatusRequestEditor = nullptr;

    std::unique_ptr<VFS_Message> genericReply(
      vfsConnections()->takeReply(requestID));
    VFS_FileStatusReply const *reply =
      genericReply->asFileStatusReplyC();
    if (reply->m_success &&
        reply->m_fileKind == SMFileUtil::FK_REGULAR) {
      if (getDocument()->m_lastFileTimestamp !=
            reply->m_fileModificationTime) {
        // Redraw the indicator of on-disk changes.
        TRACE("EditorWidget",
          "Document modTime " << getDocument()->m_lastFileTimestamp <<
          " differs from reply modTime " << reply->m_fileModificationTime <<
          ", marking as modified on disk.");
        getDocument()->m_modifiedOnDisk = true;
        this->redraw();
      }
    }
  }

  GENERIC_CATCH_END
}


void EditorWidget::on_connectionFailed(
  HostName hostName, string reason) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("EditorWidget", "on_connectionFailed: host=" << hostName <<
                        " reason=" << reason);

  // TODO: I should only cancel a request if it is being made to the
  // host that disconnected.
  cancelFileStatusRequestIfAny();

  GENERIC_CATCH_END
}


NamedTextDocument *EditorWidget::getDocument() const
{
  xassert(m_editor);
  xassert(m_editor->m_namedDoc);
  return m_editor->m_namedDoc;
}


bool EditorWidget::hasValidFileAndHostName() const
{
  NamedTextDocument const *doc = getDocument();
  return doc->hasFilename() &&
         vfsConnections()->isValid(doc->hostName());
}


TextDocumentEditor *EditorWidget::getDocumentEditor()
{
  xassert(m_editor);
  return m_editor;
}


string EditorWidget::getDocumentDirectory() const
{
  return this->getDocument()->directory();
}


HostAndResourceName EditorWidget::getDocumentDirectoryHarn() const
{
  return getDocument()->directoryHarn();
}


void EditorWidget::fileOpenAtCursor()
{
  string lineText = m_editor->getWholeLineString(m_editor->cursor().m_line);

  ArrayStack<HostAndResourceName> prefixes;
  prefixes.push(getDocumentDirectoryHarn());
  m_documentList->getUniqueDirectories(prefixes);

  VFS_QuerySync querySync(vfsConnections(), this);

  HostFileAndLineOpt hostFileAndLine =
    getNearbyFilename(querySync, prefixes,
                      lineText, m_editor->cursor().m_column);

  if (!hostFileAndLine.hasFilename()) {
    // Prompt with the document directory.
    hostFileAndLine.m_harn = getDocumentDirectoryHarn();
  }

  // Prompt the user with the filename to allow confirmation and
  // changes.
  //
  // This should be sent on a Qt::QueuedConnection, meaning the slot
  // will be invoked later, once the current event is done processing.
  // That is important because right now we have an open
  // UndoHistoryGrouper, but opening a new file might close the one we
  // are currently looking at if it is untitled, which will cause the
  // RCSerf infrastructure to abort just before memory corruption would
  // have resulted.
  Q_EMIT openFilenameInputDialogSignal(hostFileAndLine);
}


void EditorWidget::namedTextDocumentRemoved(
  NamedTextDocumentList *documentList, NamedTextDocument *file) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  xassert(documentList == m_documentList);

  // Change files if that was the one we were editing.  Do this before
  // destroying any editors.
  if (m_editor->m_namedDoc == file) {
    this->setDocumentFile(documentList->getDocumentAt(0));
  }

  // Remove 'file' from my list if I have it.
  for(ObjListMutator< NamedTextDocumentEditor > mut(m_editorList); !mut.isDone(); ) {
    if (mut.data()->m_namedDoc == file) {
      xassert(mut.data() != m_editor);
      INITIATING_DOCUMENT_CHANGE();
      mut.deleteIt();
    }
    else {
      mut.adv();
    }
  }

  GENERIC_CATCH_END
}


bool EditorWidget::getNamedTextDocumentInitialView(
  NamedTextDocumentList *documentList, NamedTextDocument *file,
  NamedTextDocumentInitialView /*OUT*/ &view) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  FOREACH_OBJLIST(NamedTextDocumentEditor, m_editorList, iter) {
    NamedTextDocumentEditor const *ed = iter.data();

    // Only return our view if it has moved away from the top
    // of the file.
    if (ed->m_namedDoc == file && !ed->firstVisible().isZero()) {
      view.firstVisible = ed->firstVisible();
      view.cursor = ed->cursor();
      return true;
    }
  }
  return false;

  GENERIC_CATCH_END_RET(false)
}


void EditorWidget::redraw()
{
  recomputeLastVisible();

  // tell our parent.. but ignore certain messages temporarily
  {
    SetRestore<bool> restore(m_ignoreScrollSignals, true);
    Q_EMIT viewChanged();
  }

  this->emitSearchStatusIndicator();
  this->computeOffscreenMatchIndicators();

  // redraw
  update();
}


QImage EditorWidget::getScreenshot()
{
  QImage image(this->size(), QImage::Format_RGB32);
  {
    QPainter paint(&image);
    this->paintFrame(paint);
  }
  return image;
}


// Compute and broadcast match status indicator.
void EditorWidget::emitSearchStatusIndicator()
{
  if (!m_textSearch->searchStringIsValid()) {
    // This is a bit crude as an error explanation, but it seems
    // adequate for an initial implementation.
    stringBuilder sb;
    sb << "Err @ " << m_textSearch->searchStringErrorOffset();
    TRACE("sar", "searchStatusIndicator: " << sb);
    Q_EMIT signal_searchStatusIndicator(toQString(sb));
    return;
  }

  // Get effective selection range for this calculation.
  TextMCoordRange range = m_editor->getSelectModelRange();

  // Matches above and below range start line.
  int matchesAbove = m_textSearch->countMatchesAbove(range.m_start.m_line);
  int matchesBelow = m_textSearch->countMatchesBelow(range.m_start.m_line);

  // Matches before, at, and after range start within its line.
  int matchesBefore=0, matchesOn=0, matchesAfter=0;

  // Number of matches exactly selected; in [0,1].
  int matchesSelected=0;

  if (m_textSearch->countLineMatches(range.m_start.m_line)) {
    ArrayStack<TextSearch::MatchExtent> const &matches =
      m_textSearch->getLineMatches(range.m_start.m_line);

    for (int i=0; i < matches.length(); i++) {
      TextSearch::MatchExtent const &m = matches[i];
      if (m.m_startByte < range.m_start.m_byteIndex) {
        matchesBefore++;
      }
      else if (m.m_startByte > range.m_start.m_byteIndex) {
        matchesAfter++;
      }
      else {
        matchesOn++;
        if (range.withinOneLine() &&
            m.m_lengthBytes == (range.m_end.m_byteIndex - range.m_start.m_byteIndex)) {
          matchesSelected++;
        }
      }
    }
  }

  /* Sample scenarios and intended presentation:
                                                  LT  on  GTE  sel
    *   hit   hit   hit             0 [] 3         0   0    3    0
       *hit   hit   hit             0 [] 3         0   1    3    0
       [hit]  hit   hit             0 [m] 3        0   1    3    1
       [h]it  hit   hit             0 [] 3         0   1    3    0
       [hit ] hit   hit             0 [] 3         0   1    3    0
        h*it  hit   hit             1 [] 2         1   0    2    0
        hit * hit   hit             1 [] 2         1   0    2    0
        hit  *hit   hit             1 [] 2         1   1    2    0
        hit   hit * hit             2 [] 1         2   0    1    0
        hit   hit  *hit             2 [] 1         2   1    1    0
        hit   hit  [hit]            2 [m] 1        2   1    1    1
        hit   hit   h*it            3 [] 0         3   0    0    0
            *                       0 [] 0         0   0    0    0
  */

  // Matches before the selection start.
  int matchesLT = matchesAbove + matchesBefore;

  // Matches at or after the selection start.
  int matchesGTE = matchesOn + matchesAfter + matchesBelow;

  stringBuilder sb;
  sb << matchesLT << " [";
  if (matchesSelected) {
    sb << 'x';
  }
  sb << "] " << matchesGTE;
  if (m_textSearch->hasIncompleteMatches()) {
    sb << '+';
  }

  TRACE("sar", "searchStatusIndicator: " << sb);
  Q_EMIT signal_searchStatusIndicator(toQString(sb));
}


void EditorWidget::computeOffscreenMatchIndicator(QLabel *label,
  int numMatches)
{
  bool incomplete = m_textSearch->hasIncompleteMatches();
  if (numMatches || incomplete) {
    char const *incompleteMarker = (incomplete? "+" : "");
    label->setText(qstringb(numMatches << incompleteMarker));
  }
  else {
    label->setText("");
  }
}

void EditorWidget::computeOffscreenMatchIndicators()
{
  this->computeOffscreenMatchIndicator(m_matchesAboveLabel,
    m_textSearch->countMatchesAbove(m_editor->firstVisible().m_line));
  this->computeOffscreenMatchIndicator(m_matchesBelowLabel,
    m_textSearch->countMatchesBelow(m_editor->lastVisible().m_line));
}


void EditorWidget::commandMoveFirstVisibleAndCursor(
  int deltaLine, int deltaCol)
{
  COMMAND_MU(EC_MoveFirstVisibleAndCursor, deltaLine, deltaCol);
}


void EditorWidget::recomputeLastVisible()
{
  int h = this->height();
  int w = this->width();

  if (m_fontHeight && m_fontWidth) {
    INITIATING_DOCUMENT_CHANGE();

    // calculate viewport size
    m_editor->setVisibleSize(
      (h - m_topMargin) / this->lineHeight(),
      (w - m_leftMargin) / m_fontWidth);
  }
  else {
    // font info not set, leave them alone
  }
}


void EditorWidget::resizeEvent(QResizeEvent *r) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QWidget::resizeEvent(r);
  this->recomputeLastVisible();
  this->computeOffscreenMatchIndicators();
  Q_EMIT viewChanged();

  GENERIC_CATCH_END
}


// In general, to avoid flickering, I try to paint every pixel
// exactly once (this idea comes straight from the Qt tutorial).
// So far, the only place I violate that is the cursor, the pixels
// of which are drawn twice when it is visible.
// UPDATE: It's irrelevant now that I've been forced into double-
// buffering by a bug in XFree86 (see redraw()).
void EditorWidget::paintEvent(QPaintEvent *ev) NOEXCEPT
{
  try {
    // draw on the pixmap
    updateFrame(ev);
  }
  catch (XBase &x) {
    // I can't pop up a message box because then when that
    // is dismissed it might trigger another exception, etc.
    QPainter paint(this);
    paint.setPen(Qt::white);
    paint.setBackgroundMode(Qt::OpaqueMode);
    paint.setBackground(Qt::red);
    paint.drawText(0, 30,                 // baseline start coordinate
                   toQString(x.why()));

    // Also write to stderr so rare issues can be seen.
    ::printUnhandled(x);
  }
}

void EditorWidget::updateFrame(QPaintEvent *ev)
{
  // debug info
  {
    string rect = "(none)";
    if (ev) {
      QRect const &r = ev->rect();
      rect = stringf("(%d,%d,%d,%d)", r.left(), r.top(),
                                      r.right(), r.bottom());
    }
    TRACE("paint", "frame: rect=" << rect);
  }

  // Painter that goes to the window directly.  A key property is that
  // every pixel painted via 'winPaint' must be painted exactly once, to
  // avoid flickering.
  QPainter winPaint(this);

  this->paintFrame(winPaint);
}

void EditorWidget::paintFrame(QPainter &winPaint)
{
  // ---- setup painters ----
  // make a pixmap, so as to avoid flickering by double-buffering; the
  // pixmap is the entire width of the window, but only one line high,
  // so as to improve drawing locality and avoid excessive allocation
  // in the server
  int const lineWidth = width();
  int const fullLineHeight = getFullLineHeight();
  QPixmap pixmap(lineWidth, fullLineHeight);

  // NOTE: This does not preclude drawing objects that span multiple
  // lines, it just means that those objects need to be drawn one line
  // segment at a time.  i.e., the interface must have clients insert
  // objects into a display list, rather than drawing arbitrary things
  // on a canvas.

  // make the main painter, which will draw on the line pixmap; the
  // font setting must be copied over manually
  QPainter paint(&pixmap);
  paint.setFont(font());

  // ---- setup style info ----
  // when drawing text, erase background automatically
  paint.setBackgroundMode(Qt::OpaqueMode);

  // Character style info.  This gets updated as we paint each line.
  LineCategories modelCategories(TC_NORMAL);

  // The style info, but expressed in layout coordinates.  For each
  // line, we first compute 'modelCategories', then 'layoutCategories'
  // is computed from the former.
  LineCategories layoutCategories(TC_NORMAL);

  // Currently selected category and style (so we can avoid possibly
  // expensive calls to change styles).
  TextCategoryAndStyle textCategoryAndStyle(
    getTextCategoryAndStyle(TC_NORMAL));
  textCategoryAndStyle.setDrawStyle(paint);

  // do same for 'winPaint', just to set the background color
  textCategoryAndStyle.setDrawStyle(winPaint);

  // ---- margins ----
  // top edge of what has not been painted, in window coordinates
  int y = 0;

  if (m_topMargin) {
    winPaint.eraseRect(0, y, lineWidth, m_topMargin);
    y += m_topMargin;
  }

  // ---- remaining setup ----
  // Visible area info.  The +1 here is to include the column after
  // the last fully visible column, which might be partially visible.
  int const visibleCols = this->visColsPlusPartial();
  int const firstCol = this->firstVisibleCol();
  int const firstLine = this->firstVisibleLine();

  // I think it might be useful to support negative values for these
  // variables, but the code below is not prepared to deal with such
  // values at this time
  xassert(firstLine >= 0);
  xassert(firstCol >= 0);

  // another sanity check
  xassert(lineHeight() > 0);

  // Buffer that will be used for each visible line of text.
  ArrayStack<char> text(visibleCols);

  // Get region of selected text.
  TextLCoordRange selRange = m_editor->getSelectLayoutRange();

  // Paint the window, one line at a time.  Both 'line' and 'y' act
  // as loop control variables.
  for (int line = firstLine;
       y < this->height();
       line++, y += fullLineHeight)
  {
    // ---- compute style segments ----
    // Number of columns from this line that are visible.
    int visibleLineCols = 0;

    // nominally the entire line is normal text
    modelCategories.clear(TC_NORMAL);
    layoutCategories.clear(TC_NORMAL);

    // This is 1 if we will behave as though a newline character is
    // at the end of this line, 0 otherwise.
    int newlineAdjust = 0;
    if (m_visibleWhitespace && line < m_editor->numLines()-1) {
      newlineAdjust = 1;
    }

    // True if the cursor is on `line`.
    bool const cursorOnCurrentLine =
      (line == m_editor->cursor().m_line);

    // Number of cells in the line, excluding newline.
    int const lineLengthColumns = m_editor->lineLengthColumns(line);

    // Column number (0-based) of first trailing whitespace character.
    // We say there is no trailing whitespace on the cursor line
    // because highlighting it there is annoying and unhelpful.
    int const startOfTrailingWhitespace =
      cursorOnCurrentLine?
        lineLengthColumns :
        lineLengthColumns - m_editor->countTrailingSpacesTabsColumns(line);

    // Number of columns with glyphs on this line, including possible
    // synthesized newline for 'visibleWhitespace'.  This value is
    // independent of the window size or scroll position.
    int const lineGlyphColumns = lineLengthColumns + newlineAdjust;

    // fill with text from the file
    text.clear();
    if (line < m_editor->numLines()) {
      if (firstCol < lineGlyphColumns) {
        // First get the text without any extra newline.
        int const amt = std::min(lineLengthColumns - firstCol, visibleCols);
        m_editor->getLineLayout(TextLCoord(line, firstCol), text, amt);
        visibleLineCols = amt;

        // Now possibly add the newline.
        if (visibleLineCols < visibleCols && newlineAdjust != 0) {
          text.push('\n');
          visibleLineCols++;
        }
      }

      // Apply syntax highlighting.
      if (m_editor->m_namedDoc->m_highlighter) {
        m_editor->m_namedDoc->m_highlighter
          ->highlightTDE(m_editor, line, /*OUT*/ modelCategories);
        m_editor->modelToLayoutSpans(line,
          /*OUT*/ layoutCategories, /*IN*/ modelCategories);
      }

      // Show search hits.
      this->addSearchMatchesToLineCategories(layoutCategories, line);
    }
    xassert(visibleLineCols <= visibleCols);
    xassert(text.length() == visibleLineCols);

    // Fill the remainder of 'text' with spaces.  These characters will
    // only be used if there is style information out beyond the actual
    // line character data.
    {
      int remainderLen = visibleCols - visibleLineCols;
      memset(text.ptrToPushedMultipleAlt(remainderLen), ' ', remainderLen);
    }
    xassert(text.length() == visibleCols);

    // incorporate effect of selection
    if (this->selectEnabled() &&
        selRange.m_start.m_line <= line && line <= selRange.m_end.m_line)
    {
      if (selRange.m_start.m_line < line && line < selRange.m_end.m_line) {
        // entire line is selected
        layoutCategories.overlay(0, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selRange.m_start.m_line < line && line == selRange.m_end.m_line) {
        // Left half of line is selected.
        if (selRange.m_end.m_column) {
          layoutCategories.overlay(0, selRange.m_end.m_column, TC_SELECTION);
        }
      }
      else if (selRange.m_start.m_line == line && line < selRange.m_end.m_line) {
        // Right half of line is selected.
        layoutCategories.overlay(selRange.m_start.m_column, 0 /*infinite*/, TC_SELECTION);
      }
      else if (selRange.m_start.m_line == line && line == selRange.m_end.m_line) {
        // Middle part of line is selected.
        if (selRange.m_end.m_column != selRange.m_start.m_column) {
          layoutCategories.overlay(selRange.m_start.m_column,
            selRange.m_end.m_column - selRange.m_start.m_column, TC_SELECTION);
        }
      }
      else {
        xfailure("messed up my logic");
      }
    }

    // Iterator over line contents.  This is partially redundant with
    // what is in 'text', but needed to handle glyphs that span columns.
    // Perhaps I should remove 'text' at some point.
    TextDocumentEditor::LineIterator lineIter(*m_editor, line);
    while (lineIter.has() && lineIter.columnOffset() < firstCol) {
      lineIter.advByte();
    }

    // Given that we have chosen how to render the line, storing that
    // information primarily into `text` (chars to draw) and
    // `layoutCategories` (how to draw them), draw the line to `paint`.
    paintOneLine(
      paint,
      lineGlyphColumns,
      visibleLineCols,
      startOfTrailingWhitespace,
      layoutCategories,
      text,
      std::move(lineIter),
      /*INOUT*/ textCategoryAndStyle);

    // Draw the cursor on the line it is on.
    if (cursorOnCurrentLine) {
      drawCursorOnLine(
        paint,
        layoutCategories,
        text,
        lineGlyphColumns);
    }

    drawSoftMarginIndicator(paint);

    // draw the line buffer to the window
    winPaint.drawPixmap(0,y, pixmap);    // draw it
  }

  // Also draw indicators of number of matches offscreen.
  this->drawOffscreenMatchIndicators(winPaint);
}


void EditorWidget::paintOneLine(
  QPainter &paint,

  // Number of columns with glyphs on this line, including possible
  // synthesized newline for 'visibleWhitespace'.  This value is
  // independent of the window size or scroll position.
  int lineGlyphColumns,

  // Number of columns from this line that are visible, including the
  // possible synthetic newline.
  int visibleLineCols,

  int startOfTrailingWhitespace,
  LineCategories const &layoutCategories,
  ArrayStack<char> const &text,
  TextDocumentEditor::LineIterator &&lineIter,
  TextCategoryAndStyle /*INOUT*/ &textCategoryAndStyle)
{
  int const lineWidth = width();
  int const fullLineHeight = getFullLineHeight();
  int const visibleCols = visColsPlusPartial();
  int const firstCol = firstVisibleCol();

  // Clear the left margin to the normal background color.
  textCategoryAndStyle.setDrawStyleIfNewCategory(paint, TC_NORMAL);
  paint.eraseRect(0,0, m_leftMargin, fullLineHeight);

  // Next category entry to use.
  LineCategoryIter category(layoutCategories);
  category.advanceCharsOrCols(firstCol);

  // ---- render text+style segments -----
  // right edge of what has not been painted, relative to
  // the pixels in the pixmap
  int x = m_leftMargin;

  // Number of columns printed.
  int printedCols = 0;

  // 'y' coordinate of the origin point of characters
  int baseline = getBaselineYCoordWithinLine();

  // loop over segments with different styles
  while (x < lineWidth) {
    if (!( printedCols < visibleCols )) {
      // This happens if we are asked to paint before the visible
      // region calculation runs.  That is not supposed to happen
      // normally, but failing an assertion in the paint routine
      // causes trouble, and I can easily cope with it here.
      break;
    }

    // set style
    textCategoryAndStyle.setDrawStyleIfNewCategory(paint,
      category.category);

    // compute how many characters to print in this segment
    int len = category.length;
    if (category.length == 0) {
      // actually means infinite length
      if (printedCols >= visibleLineCols) {
        // we've printed all the interesting characters on this line
        // because we're past the end of the line's chars, and we're
        // on the last style run; for efficiency of communication
        // with the X server, render the remainder of this line with
        // a single rectangle
        paint.eraseRect(x,0, lineWidth-x, fullLineHeight);
        break;   // out of loop over line segments
      }

      // print only the remaining chars on the line, to improve
      // the chances we'll use the eraseRect() optimization above
      len = visibleLineCols-printedCols;
    }
    len = std::min(len, visibleCols-printedCols);
    xassert(len > 0);

    // The QtBDFFont package must be treated as if it draws
    // characters with transparency, even though the transparency
    // is only partial...
    paint.eraseRect(x,0, m_fontWidth*len, fullLineHeight);

    // draw text
    int const charsToDraw = std::min(len, (lineGlyphColumns-firstCol)-printedCols);
    for (int i=0; i < charsToDraw; i++) {
      if (lineIter.has()) {
        if (lineIter.columnOffset() > firstCol+printedCols+i) {
          // This column is part of a multicolumn glyph.  Do not
          // draw any glyph here.
          continue;
        }
        xassert(lineIter.columnOffset() == firstCol+printedCols+i);
        lineIter.advByte();
      }
      else if (text[printedCols+i] != '\n') {
        // The only thing we should need to print beyond what is in
        // the line iterator is a newline, so skip drawing here.
        continue;
      }

      bool withinTrailingWhitespace =
        firstCol + printedCols + i >= startOfTrailingWhitespace;
      this->drawOneChar(paint,
                        textCategoryAndStyle.getFont(),
                        QPoint(x + m_fontWidth*i, baseline),
                        text[printedCols+i],
                        withinTrailingWhitespace);

    } // character loop (within segment)

    if (textCategoryAndStyle.underlining()) {
      drawUnderline(paint, x, len);
    }

    // Advance to next category segment.
    x += m_fontWidth * len;
    printedCols += len;
    category.advanceCharsOrCols(len);

  } // segment loop
}


void EditorWidget::drawUnderline(QPainter &paint, int x, int numCols)
{
  int const baseline = getBaselineYCoordWithinLine();

  // want to draw a line on top of where underscores would be; this
  // might not be consistent across fonts, so I might want to have
  // a user-specifiable underlining offset.. also, I don't want this
  // going into the next line, so truncate according to descent
  int ulBaseline = baseline + std::min(UNDERLINE_OFFSET, m_fontDescent);
  paint.drawLine(x, ulBaseline, x + m_fontWidth*numCols, ulBaseline);
}


void EditorWidget::drawCursorOnLine(
  QPainter &paint,
  LineCategories const &layoutCategories,
  ArrayStack<char> const &text,
  int lineGlyphColumns)
{
  // just testing the mechanism that catches exceptions
  // raised while drawing
  //if (line == 5) {
  //  THROW(XBase("aiyee! sample exception!"));
  //}

  paint.save();

  int const visibleCols = visColsPlusPartial();

  // 0-based cursor column relative to what is visible
  int const firstCol = firstVisibleCol();
  int const cursorCol = m_editor->cursor().m_column;
  int const visibleCursorCol = cursorCol - firstCol;

  // 'x' coordinate of the leftmost column of the character cell
  // where the cursor is, i.e., the character that would be deleted
  // if the Delete key were pressed.
  int x = m_leftMargin + m_fontWidth * visibleCursorCol;

  if (visibleCursorCol < 0) {
    // The cursor is off the left edge, so nothing to show.
  }
  else if (visibleCursorCol >= visibleCols) {
    // Cursor off right edge, also skip.
  }
  else if (false) {     // thin vertical bar
    paint.setPen(m_cursorColor);
    paint.drawLine(x,0, x, m_fontHeight-1);
    paint.drawLine(x-1,0, x-1, m_fontHeight-1);
  }
  else if (!this->hasFocus()) {
    // emacs-like non-focused unfilled box.
    paint.setPen(m_cursorColor);
    paint.setBrush(QBrush());

    // Setting the pen width to 2 does not produce a good result,
    // so just draw two 1-pixel rectangles.
    paint.drawRect(x,   0, m_fontWidth,   m_fontHeight-1);
    paint.drawRect(x+1, 1, m_fontWidth-2, m_fontHeight-3);
  }
  else {           // emacs-like box
    StyleDB const *styleDB = StyleDB::instance();
    int const baseline = getBaselineYCoordWithinLine();

    // The character shown inside the box should use the same
    // font as if it were not inside the cursor box, to minimize
    // the visual disruption caused by the cursor's presence.
    //
    // Unfortunately, that leads to some code duplication with the
    // main painting code.
    TextCategory cursorCategory = layoutCategories.getCategoryAt(cursorCol);
    FontVariant cursorFV = styleDB->getStyle(cursorCategory).variant;
    bool underlineCursor = false;
    if (cursorFV == FV_UNDERLINE) {
      cursorFV = FV_NORMAL;   // 'cursorFontForFV' does not map FV_UNDERLINE
      underlineCursor = true;
    }
    QtBDFFont *cursorFont = m_cursorFontForFV[cursorFV];

    paint.setBackground(cursorFont->getBgColor());
    paint.eraseRect(x,0, m_fontWidth, m_fontHeight);

    if (cursorCol < lineGlyphColumns) {
      // Drawing the block cursor overwrote the glyph, so we
      // have to draw it again.
      if (text[visibleCursorCol] == ' ' &&
          !m_editor->cursorOnModelCoord()) {
        // This is a layout placeholder space, not really present in
        // the document, so don't draw it.
      }
      else {
        this->drawOneChar(paint, cursorFont, QPoint(x, baseline),
                          text[visibleCursorCol],
                          false /*withinTrailingWhitespace*/);
      }
    }

    if (underlineCursor) {
      paint.setPen(cursorFont->getFgColor());
      drawUnderline(paint, x, 1 /*numCols*/);
    }
  }

  paint.restore();
}


void EditorWidget::drawSoftMarginIndicator(QPainter &paint)
{
  if (m_visibleSoftMargin) {
    paint.save();
    paint.setPen(m_softMarginColor);

    int firstCol = firstVisibleCol();
    int x = m_leftMargin + m_fontWidth * (m_softMarginColumn - firstCol);
    paint.drawLine(x, 0, x, m_fontHeight-1);

    paint.restore();
  }
}


void EditorWidget::drawOneChar(QPainter &paint, QtBDFFont *font,
  QPoint const &pt, char c, bool withinTrailingWhitespace)
{
  // My document representation uses 'char' without much regard
  // to character encoding.  Here, I'm declaring that this whole
  // time I've been storing some 8-bit encoding consistent with
  // the font I'm using, which is Latin-1.  At some point I need
  // to develop and implement a character encoding strategy.
  int codePoint = (unsigned char)c;

  if (codePoint==' ' || codePoint=='\n' ||
      codePoint=='\r' || codePoint=='\t') {
    if (!m_visibleWhitespace) {
      return;           // Nothing to draw.
    }

    QRect bounds = font->getNominalCharCell(pt);
    QColor fg = font->getFgColor();
    fg.setAlpha(m_whitespaceOpacity);

    // Optionally highlight trailing whitespace (but not line
    // terminator characters).
    if (withinTrailingWhitespace &&
        (codePoint == ' ' || codePoint=='\t') &&
        m_editor->m_namedDoc->m_highlightTrailingWhitespace) {
      paint.fillRect(bounds, m_trailingWhitespaceBgColor);
    }

    if (codePoint == ' ') {
      // Centered dot.
      paint.fillRect(QRect(bounds.center(), QSize(2,2)), fg);
      return;
    }

    if (codePoint == '\n' || codePoint == '\r' || codePoint == '\t') {
      // Filled triangle.
      int x1 = bounds.left() + bounds.width() * 1/8;
      int x7 = bounds.left() + bounds.width() * 7/8;
      int y1 = bounds.top() + bounds.height() * 1/8;
      int y4 = bounds.top() + bounds.height() * 4/8;
      int y7 = bounds.top() + bounds.height() * 7/8;

      paint.setPen(Qt::NoPen);
      paint.setBrush(fg);

      if (codePoint == '\n') {
        // Lower-right.
        QPoint pts[] = {
          QPoint(x1,y7),
          QPoint(x7,y1),
          QPoint(x7,y7),
        };
        paint.drawConvexPolygon(pts, TABLESIZE(pts));
      }
      else if (codePoint == '\r') {
        // Upper-left.
        QPoint pts[] = {
          QPoint(x1,y7),
          QPoint(x1,y1),
          QPoint(x7,y1),
        };
        paint.drawConvexPolygon(pts, TABLESIZE(pts));
      }
      else /*Tab*/ {
        // Right-arrow.
        QPoint pts[] = {
          QPoint(x1,y1),
          QPoint(x7,y4),
          QPoint(x1,y7),
        };
        paint.drawConvexPolygon(pts, TABLESIZE(pts));
      }
      return;
    }
  }

  if (font->hasChar(codePoint)) {
    font->drawChar(paint, pt, codePoint);
  }
  else {
    QRect bounds = font->getNominalCharCell(pt);

    // This is a somewhat expensive thing to do because it requires
    // re-rendering the offscreen glyphs.  Hence, I only do it once
    // I know I need it.
    m_minihexFont->setSameFgBgColors(*font);

    drawHexQuad(*(m_minihexFont), paint, bounds, codePoint);
  }
}


TextCategoryAndStyle EditorWidget::getTextCategoryAndStyle(
  TextCategory cat) const
{
  return TextCategoryAndStyle(
    m_fontForCategory,
    cat,
    getDocument()->m_modifiedOnDisk /*useDarker*/);
}


void EditorWidget::drawOffscreenMatchIndicators(QPainter &paint)
{
  // Use the same appearance as search hits, as that will help convey
  // what the numbers mean.
  TextCategoryAndStyle tcas(getTextCategoryAndStyle(TC_HITS));
  tcas.setDrawStyle(paint);

  this->drawOneCornerLabel(paint, tcas.m_font,
    false /*isLeft*/, true /*isTop*/, m_matchesAboveLabel->text());
  this->drawOneCornerLabel(paint, tcas.m_font,
    false /*isLeft*/, false /*isTop*/, m_matchesBelowLabel->text());
}


void EditorWidget::drawOneCornerLabel(
  QPainter &paint, QtBDFFont *font, bool isLeft, bool isTop,
  QString const &text)
{
  if (text.isEmpty()) {
    return;
  }

  string s(toString(text));
  int labelWidth = m_fontWidth * s.length();

  // This uses the left/top margins for bottom/right in order to achieve
  // a symmetric appearance.
  int leftEdge = isLeft?
    m_leftMargin :
    this->width() - labelWidth - m_leftMargin;
  int topEdge = isTop?
    m_topMargin :
    this->height() - m_fontHeight - m_topMargin;

  QRect rect(leftEdge, topEdge, labelWidth, m_fontHeight);
  paint.eraseRect(rect);

  int baseline = getBaselineYCoordWithinLine();
  drawString(*font, paint, QPoint(leftEdge, topEdge+baseline), s);
}


void EditorWidget::addSearchMatchesToLineCategories(
  LineCategories &categories, int line)
{
  if (m_textSearch->countLineMatches(line)) {
    ArrayStack<TextSearch::MatchExtent> const &matches =
      m_textSearch->getLineMatches(line);
    for (int i=0; i < matches.length(); i++) {
      TextSearch::MatchExtent const &m = matches[i];
      if (m.m_lengthBytes) {
        // Convert match extent to layout coordinates since
        // 'categories' is indexed by column, not byte.
        TextMCoordRange mrange(
          TextMCoord(line, m.m_startByte),
          TextMCoord(line, m.m_startByte + m.m_lengthBytes));
        TextLCoordRange lrange(m_editor->toLCoordRange(mrange));
        int columns = lrange.m_end.m_column - lrange.m_start.m_column;

        // Double-check that the match is not zero columns.
        // Currently this cannot happen (if 'm_lengthBytes' is not
        // zero), but it will become possible if I lay out
        // zero-width characters properly.
        if (columns) {
          categories.overlay(lrange.m_start.m_column, columns, TC_HITS);
        }
      }
      else {
        // LineCategories::overlay() interprets a zero length as
        // meaning "infinite".  I don't currently have a good way to
        // show 0-length matches, which are possible when using
        // regexes, so I will just not show them.  It is still
        // possible to step through them with next/prev match,
        // though.
      }
    }
  }
}


// increment, but don't allow result to go below 0
static void inc(int &val, int amt)
{
  val += amt;
  if (val < 0) {
    val = 0;
  }
}


void EditorWidget::keyPressEvent(QKeyEvent *k) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("input", "keyPress: " << keysString(*k));

  if (!this->hasFocus()) {
    // See doc/qt-focus-issues.txt.  This is a weird state, but I go
    // ahead anyway since my design is intended to be robust against
    // Qt screwing up its focus data and notifications.
    TRACE("focus", "got a keystroke but I do not have focus!");

    // Nevertheless, I do not want to leave things this way because the
    // menu looks weird and there might be other issues.  This seems to
    // be an effective way to repair this screwy state.
    this->setFocus(Qt::PopupFocusReason);

    // At this point the menu bar is still grayed out, but that can be
    // fixed with a repaint.
    this->window()->update();
  }

  // This is the single most important place to ensure I do not act upon
  // document change notifications.
  //
  // TODO: Once I transition to doing all modifications using the
  // "command" infrastructure, I can remove this.
  INITIATING_DOCUMENT_CHANGE();

  // TODO: I think this grouper should be removed in favor of whatever
  // is needed inside the command object handler.
  UndoHistoryGrouper hbgrouper(*m_editor);

  Qt::KeyboardModifiers modifiers = k->modifiers();

  // Ctrl+<key>
  if (modifiers == Qt::ControlModifier) {
    switch (k->key()) {
      case Qt::Key_Insert:
        commandEditCopy();
        break;

      case Qt::Key_PageUp:
        COMMAND_MU(EC_MoveCursorToFileExtremum,
          true /*start*/, false /*select*/);
        break;

      case Qt::Key_PageDown:
        COMMAND_MU(EC_MoveCursorToFileExtremum,
          false /*start*/, false /*select*/);
        break;

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        COMMAND_MU(EC_CursorToEndOfNextLine, false);
        break;
      }

      case Qt::Key_W:
        COMMAND_MU(EC_MoveFirstVisibleConfineCursor, -1, 0);
        break;

      case Qt::Key_Z:
        COMMAND_MU(EC_MoveFirstVisibleConfineCursor, +1, 0);
        break;

      case Qt::Key_Up:
        commandMoveFirstVisibleAndCursor(-1, 0);
        break;

      case Qt::Key_Down:
        commandMoveFirstVisibleAndCursor(+1, 0);
        break;

      case Qt::Key_Left:
        commandMoveFirstVisibleAndCursor(0, -1);
        break;

      case Qt::Key_Right:
        commandMoveFirstVisibleAndCursor(0, +1);
        break;

      case Qt::Key_B:      commandCursorLeft(false); break;
      case Qt::Key_F:      commandCursorRight(false); break;
      case Qt::Key_A:      commandCursorHome(false); break;
      case Qt::Key_E:      commandCursorEnd(false); break;
      case Qt::Key_P:      commandCursorUp(false); break;
      case Qt::Key_N:      commandCursorDown(false); break;
      // emacs' pageup/pagedown are ctrl-v and alt-v, but the
      // latter should be reserved for accessing the menu, so I'm
      // not going to bind either by default

      case Qt::Key_D:
        COMMAND_MU(EC_DeleteKeyFunction);
        break;

      case Qt::Key_H:
        COMMAND_MU(EC_BackspaceFunction);
        break;

      case Qt::Key_L:
        COMMAND_MU(EC_CenterVisibleOnCursorLine);
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Alt+<key>
  else if (modifiers == Qt::AltModifier) {
    switch (k->key()) {
      case Qt::Key_Left:
        this->commandEditRigidUnindent();
        break;

      case Qt::Key_Right:
        this->commandEditRigidIndent();
        break;
    }
  }

  // Ctrl+Alt+<key>.  This is where I put commands mainly meant for
  // use while debugging, although Ctrl+Alt+Left/Right (which are
  // handled as menu shortcuts) are ordinary commands.  I recently
  // learned that Ctrl+Alt is used on some keyboards to compose more
  // complex characters, so it is probably best to avoid adding many
  // keybindings for it.
  else if (modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
    switch (k->key()) {
      case Qt::Key_B: {
        breaker();     // breakpoint for debugger
        break;
      }

      case Qt::Key_X: {
        // test exception mechanism...
        THROW(XMessage("gratuitous exception"));
        break;
      }

      case Qt::Key_W:
        DEV_WARNING("Synthetic DEV_WARNING due to Ctrl+Alt+W");
        break;

      case Qt::Key_Y: {
        try {
          xmessage("another exc");
        }
        catch (XBase &x) {
          QMessageBox::information(this, "got it",
            "got it");
        }
        break;
      }

      case Qt::Key_P: {
        long start = getMilliseconds();
        int frames = 20;
        for (int i=0; i < frames; i++) {
          redraw();
        }
        long elapsed = getMilliseconds() - start;
        QMessageBox::information(this, "perftest",
          qstringb("drew " << frames << " frames in " <<
                   elapsed << " milliseconds, or " <<
                   (elapsed / frames) << " ms/frame"));
        break;
      }

      case Qt::Key_U:
        m_editor->debugPrint();
        break;

      case Qt::Key_H:
        m_editor->printHistory();
        m_editor->printHistoryStats();
        break;

      default:
        k->ignore();
        break;
    }
  }

  // Ctrl+Shift+<key>
  else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
    switch (k->key()) {
      case Qt::Key_Up:
        commandMoveFirstVisibleAndCursor(-CTRL_SHIFT_DISTANCE, 0);
        break;

      case Qt::Key_Down:
        commandMoveFirstVisibleAndCursor(+CTRL_SHIFT_DISTANCE, 0);
        break;

      case Qt::Key_Left:
        commandMoveFirstVisibleAndCursor(0, -CTRL_SHIFT_DISTANCE);
        break;

      case Qt::Key_Right:
        commandMoveFirstVisibleAndCursor(0, +CTRL_SHIFT_DISTANCE);
        break;

      case Qt::Key_PageUp:
        COMMAND_MU(EC_MoveCursorToFileExtremum,
          true /*start*/, true /*select*/);
        break;

      case Qt::Key_PageDown:
        COMMAND_MU(EC_MoveCursorToFileExtremum,
          false /*start*/, true /*select*/);
        break;

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        commandCursorToEndOfNextLine(true);
        break;
      }

      case Qt::Key_B:      commandCursorLeft(true); break;
      case Qt::Key_F:      commandCursorRight(true); break;
      case Qt::Key_A:      commandCursorHome(true); break;
      case Qt::Key_E:      commandCursorEnd(true); break;
      case Qt::Key_P:      commandCursorUp(true); break;
      case Qt::Key_N:      commandCursorDown(true); break;

      default:
        k->ignore();
        break;
    }
  }

  // <key> and shift-<key>
  else if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier) {
    bool shift = (modifiers == Qt::ShiftModifier);
    switch (k->key()) {
      case Qt::Key_Insert:
        if (shift) {
          commandEditPaste();
        }
        else {
          // TODO: toggle insert/overwrite mode
        }
        break;

      case Qt::Key_Left:     commandCursorLeft(shift); break;
      case Qt::Key_Right:    commandCursorRight(shift); break;
      case Qt::Key_Home:     commandCursorHome(shift); break;
      case Qt::Key_End:      commandCursorEnd(shift); break;
      case Qt::Key_Up:       commandCursorUp(shift); break;
      case Qt::Key_Down:     commandCursorDown(shift); break;
      case Qt::Key_PageUp:   commandCursorPageUp(shift); break;
      case Qt::Key_PageDown: commandCursorPageDown(shift); break;

      case Qt::Key_Backspace: {
        if (!editSafetyCheck()) {
          return;
        }
        if (shift) {
          // I'm deliberately leaving Shift+Backspace unbound in
          // case I want to use it for something else later.
        }
        else {
          COMMAND_MU(EC_BackspaceFunction);
        }
        break;
      }

      case Qt::Key_Delete: {
        if (!this->editSafetyCheck()) {
          return;
        }
        if (shift) {
          this->commandEditCut();
        }
        else {
          COMMAND_MU(EC_DeleteKeyFunction);
        }
        break;
      }

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        if (shift) {
          // Shift+Enter is deliberately left unbound.
        }
        else {
          if (this->editSafetyCheck()) {
            COMMAND_MU(EC_InsertNewlineAutoIndent);
          }
        }
        break;
      }

      case Qt::Key_Tab: {
        if (this->editSafetyCheck()) {
          if (shift) {
            // In my testing on Windows, this does not get executed,
            // rather Shift+Tab is delivered as Key_Backtab.  I do not
            // know if the same is true on Linux and Mac, so I will
            // leave this here just in case.
            this->commandEditRigidUnindent();
          }
          else if (this->selectEnabled()) {
            this->commandEditRigidIndent();
          }
          else {
            COMMAND_MU(EC_InsertString, std::string("\t"));
          }
        }
        break;
      }

      case Qt::Key_Backtab: {
        this->commandEditRigidUnindent();
        break;
      }

      case Qt::Key_Escape:
        if (!shift) {
          this->doCloseSARPanel();
        }
        break;

      default: {
        QString text = k->text();
        if (text.length() && text[0].isPrint()) {
          if (!editSafetyCheck()) {
            return;
          }

          // Insert this character at the cursor.
          COMMAND_MU(EC_InsertString, toString(text));
        }
        else {
          k->ignore();
          return;
        }
      }
    }
  }

  // other combinations
  else {
    k->ignore();
  }

  GENERIC_CATCH_END
}


void EditorWidget::keyReleaseEvent(QKeyEvent *k) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("input", "keyRelease: " << keysString(*k));

  // Not sure if this is the best place for this, but it seems
  // worth a try.
  this->selfCheck();

  k->ignore();

  GENERIC_CATCH_END
}


void EditorWidget::scrollToCursor(int edgeGap)
{
  INITIATING_DOCUMENT_CHANGE();
  m_editor->scrollToCursor(edgeGap);
  redraw();
}


void EditorWidget::scrollToLine(int line)
{
  INITIATING_DOCUMENT_CHANGE();
  if (!m_ignoreScrollSignals) {
    xassert(line >= 0);
    m_editor->setFirstVisibleLine(line);
    redraw();
  }
}

void EditorWidget::scrollToCol(int col)
{
  INITIATING_DOCUMENT_CHANGE();
  if (!m_ignoreScrollSignals) {
    xassert(col >= 0);
    m_editor->setFirstVisibleCol(col);
    redraw();
  }
}


void EditorWidget::setCursorToClickLoc(QMouseEvent *m)
{
  int x = m->x();
  int y = m->y();

  // subtract off the margin, but don't let either coord go negative
  inc(x, -m_leftMargin);
  inc(y, -m_topMargin);

  int newLine = y/lineHeight() + this->firstVisibleLine();
  int newCol = x/m_fontWidth + this->firstVisibleCol();

  cursorTo(TextLCoord(newLine, newCol));

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
}


void EditorWidget::mousePressEvent(QMouseEvent *m) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // get rid of popups?
  QWidget::mousePressEvent(m);
  INITIATING_DOCUMENT_CHANGE();

  m_editor->turnSelection(!!(m->modifiers() & Qt::ShiftModifier));
  setCursorToClickLoc(m);

  redraw();

  GENERIC_CATCH_END
}


void EditorWidget::mouseMoveEvent(QMouseEvent *m) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QWidget::mouseMoveEvent(m);
  INITIATING_DOCUMENT_CHANGE();

  m_editor->turnOnSelection();
  setCursorToClickLoc(m);
  m_editor->turnOffSelectionIfEmpty();

  redraw();

  GENERIC_CATCH_END
}


void EditorWidget::mouseReleaseEvent(QMouseEvent *m) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QWidget::mouseReleaseEvent(m);
  INITIATING_DOCUMENT_CHANGE();

  m_editor->turnOnSelection();
  setCursorToClickLoc(m);
  m_editor->turnOffSelectionIfEmpty();

  redraw();

  GENERIC_CATCH_END
}


// ----------------------- edit menu -----------------------
void EditorWidget::editUndo()
{
  INITIATING_DOCUMENT_CHANGE();
  if (m_editor->canUndo()) {
    m_editor->undo();
    this->redraw();
  }
  else {
    QMessageBox::information(this, "Can't undo",
      "There are no actions to undo in the history.");
  }
}


void EditorWidget::editRedo()
{
  INITIATING_DOCUMENT_CHANGE();
  if (m_editor->canRedo()) {
    m_editor->redo();
    this->redraw();
  }
  else {
    QMessageBox::information(this, "Can't redo",
      "There are no actions to redo in the history.");
  }
}


static void setClipboard(string newText)
{
  QClipboard *cb = QApplication::clipboard();
  cb->setText(toQString(newText), QClipboard::Clipboard);
  if (cb->supportsSelection()) {
    // Also set the X selection so I can paste it into an xterm.
    cb->setText(toQString(newText), QClipboard::Selection);
  }
}


void EditorWidget::commandEditCut()
{
  if (editSafetyCheck()) {
    COMMAND_MU(EC_Cut);
  }
}


void EditorWidget::commandEditCopy()
{
  COMMAND_MU(EC_Copy);
}


void EditorWidget::commandEditPaste()
{
  if (editSafetyCheck()) {
    COMMAND_MU(EC_Paste);
  }
}


void EditorWidget::commandEditDelete()
{
  if (editSafetyCheck()) {
    COMMAND_MU(EC_DeleteMenuFunction);
  }
}


void EditorWidget::commandEditKillLine()
{
  if (!editSafetyCheck()) {
    return;
  }
  COMMAND_MU(EC_KillLine);
}


void EditorWidget::commandEditSelectEntireFile()
{
  COMMAND_MU(EC_SelectEntireFile);
}


void EditorWidget::showInfo(char const *infoString)
{
  QWidget *main = this->window();

  if (!m_infoBox) {
    m_infoBox = new QLabel(main);
    m_infoBox->setObjectName("infoBox");
    m_infoBox->setForegroundRole(QPalette::ToolTipText);
    m_infoBox->setBackgroundRole(QPalette::ToolTipBase);
    m_infoBox->setAutoFillBackground(true);
    m_infoBox->setIndent(2);
  }

  m_infoBox->setText(infoString);

  // compute a good size for the label
  QFontMetrics fm(m_infoBox->font());
  QSize sz = fm.size(0, infoString);
  m_infoBox->resize(sz.width() + 4, sz.height() + 2);

  // Compute a position just below the lower-left corner
  // of the cursor box, in the coordinates of 'this'.
  QPoint target(
    (cursorCol() - this->firstVisibleCol()) * m_fontWidth,
    (cursorLine() - this->firstVisibleLine() + 1) * m_fontHeight + 1);

  // Translate that to the coordinates of 'main'.
  target = this->mapTo(main, target);
  m_infoBox->move(target);

  // If the box goes beyond the right edge of the window, pull it back
  // to the left to keep it inside.
  if (m_infoBox->x() + m_infoBox->width() > main->width()) {
    m_infoBox->move(main->width() - m_infoBox->width(), m_infoBox->y());
  }

  m_infoBox->show();
}

void EditorWidget::hideInfo()
{
  if (m_infoBox) {
    delete m_infoBox;
    m_infoBox = NULL;
  }
}


bool EditorWidget::highlightTrailingWhitespace() const
{
  return m_editor->m_namedDoc->m_highlightTrailingWhitespace;
}

void EditorWidget::toggleHighlightTrailingWhitespace()
{
  m_editor->m_namedDoc->m_highlightTrailingWhitespace =
    !m_editor->m_namedDoc->m_highlightTrailingWhitespace;
}


void EditorWidget::commandCursorLeft(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell, 0, -1, shift);
}

void EditorWidget::commandCursorRight(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell, 0, +1, shift);
}

void EditorWidget::commandCursorHome(bool shift)
{
  COMMAND_MU(EC_MoveCursorToLineExtremum,
    true /*start*/, shift);
}

void EditorWidget::commandCursorEnd(bool shift)
{
  COMMAND_MU(EC_MoveCursorToLineExtremum,
    false /*start*/, shift);
}

void EditorWidget::commandCursorUp(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell, -1, 0, shift);
}

void EditorWidget::commandCursorDown(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell, +1, 0, shift);
}

void EditorWidget::commandCursorPageUp(bool shift)
{
  COMMAND_MU(EC_MoveCursorByPage, -1, shift);
}

void EditorWidget::commandCursorPageDown(bool shift)
{
  COMMAND_MU(EC_MoveCursorByPage, +1, shift);
}


void EditorWidget::commandCursorToEndOfNextLine(bool shift)
{
  COMMAND_MU(EC_CursorToEndOfNextLine, shift);
}


void EditorWidget::initCursorForProcessOutput()
{
  // Start by making the start of the document visible.
  m_editor->setFirstVisible(TextLCoord(0,0));

  // Jump to the end of the document.  Even for a new process document,
  // there are a few lines of status information at the top.
  m_editor->moveCursorToBottom();
  m_editor->clearMark();

  // Bring the cursor line into view.
  m_editor->scrollToCursor();
}


void EditorWidget::setTextSearchParameters()
{
  m_textSearch->setSearchStringAndFlags(m_hitText, m_hitTextFlags);
}


void EditorWidget::setSearchStringParams(string const &searchString,
  TextSearch::SearchStringFlags flags, bool scrollToHit)
{
  TRACE("sar", "EW::setSearchStringParams: str=\"" << searchString <<
               "\" flags=" << flags << " scroll=" << scrollToHit);

  m_hitText = searchString;
  m_hitTextFlags = flags;

  this->setTextSearchParameters();

  if (scrollToHit) {
    // Find the first occurrence on or after the cursor; or, failing
    // that, first occurrence before it.
    this->scrollToNextSearchHit(false /*reverse*/, false /*select*/) ||
      this->scrollToNextSearchHit(true /*reverse*/, false /*select*/);
  }

  redraw();
}


bool EditorWidget::scrollToNextSearchHit(bool reverse, bool select)
{
  TextMCoordRange modelRange = m_editor->getSelectModelRange();

  if (m_textSearch->nextMatch(reverse, modelRange)) {
    TRACE("sar", (reverse? "prev" : "next") <<
                 " found model range: " << modelRange);

    TextLCoordRange layoutRange(m_editor->toLCoordRange(modelRange));
    if (select) {
      m_editor->setSelectRange(layoutRange);
    }

    // Try to show the entire match, giving preference to the end.
    m_editor->scrollToCoord(layoutRange.m_start, SAR_SCROLL_GAP);
    m_editor->scrollToCoord(layoutRange.m_end, SAR_SCROLL_GAP);
    return true;
  }
  else {
    TRACE("sar", (reverse? "prev" : "next") << " did not find anything");
    return false;
  }
}


bool EditorWidget::nextSearchHit(bool reverse)
{
  if (this->scrollToNextSearchHit(reverse, true /*select*/)) {
    redraw();
    return true;
  }
  else {
    return false;
  }
}


void EditorWidget::replaceSearchHit(string const &replaceSpec)
{
  string existing = this->getSelectedText();
  string replacement =
    m_textSearch->getReplacementText(existing, replaceSpec);

  TRACE("sar", "replace: " << DEBUG_VALUES3(existing, replaceSpec, replacement));

  INITIATING_DOCUMENT_CHANGE();
  UndoHistoryGrouper ugh(*m_editor);
  m_editor->insertString(replacement);

  // If we are replacing at EOL, then advance to the next line so we do
  // not repeatedly replace the same EOL.
  if (m_textSearch->searchStringEndsWithEOL()) {
    m_editor->moveToNextLineStart();
  }

  redraw();
}


bool EditorWidget::searchHitSelected() const
{
  TextMCoordRange range = m_editor->getSelectModelRange();
  return m_textSearch->rangeIsMatch(range.m_start, range.m_end);
}


void EditorWidget::doCloseSARPanel()
{
  m_hitText = "";
  this->setTextSearchParameters();
  this->computeOffscreenMatchIndicators();
  Q_EMIT closeSARPanel();
  update();
}


void EditorWidget::commandBlockIndent(int amt)
{
  COMMAND_MU(EC_BlockIndent, amt);
}


void EditorWidget::editJustifyParagraph()
{
  if (!editSafetyCheck()) {
    return;
  }
  if (selectEnabled()) {
    // TODO: This.
    QMessageBox::information(this, "Unimplemented",
      "Unimplemented: justification of selected region.");
  }
  else {
    INITIATING_DOCUMENT_CHANGE();
    UndoHistoryGrouper ugh(*m_editor);
    m_editor->justifyNearCursor(m_softMarginColumn);
    this->redraw();
  }
}


void EditorWidget::editInsertDateTime()
{
  INITIATING_DOCUMENT_CHANGE();
  UndoHistoryGrouper ugh(*m_editor);
  m_editor->insertDateTime();
  this->redraw();
}


void EditorWidget::insertText(char const *text, int length,
                              TextDocumentEditor::InsertTextFlags flags)
{
  INITIATING_DOCUMENT_CHANGE();
  UndoHistoryGrouper ugh(*m_editor);
  m_editor->insertText(text, length, flags);
  this->redraw();
}


// ----------------- nonfocus situation ------------------
void EditorWidget::focusInEvent(QFocusEvent *e) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("focus", "editor(" << (void*)this << "): focus in");
  QWidget::focusInEvent(e);

  // Refreshing when we gain focus interacts badly with the window that
  // pops up when a VFS operation is delayed.  Let's turn this off for
  // now.
  //this->requestFileStatus();

  GENERIC_CATCH_END
}


void EditorWidget::focusOutEvent(QFocusEvent *e) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("focus", "editor(" << (void*)this << "): focus out");
  QWidget::focusOutEvent(e);

  GENERIC_CATCH_END
}


VFS_Connections *EditorWidget::vfsConnections() const
{
  return m_editorWindow->vfsConnections();
}


void EditorWidget::stopListening()
{
  INITIATING_DOCUMENT_CHANGE();
  if (m_listening) {
    m_editor->removeObserver(this);
    m_listening = false;
  }
}

void EditorWidget::startListening()
{
  INITIATING_DOCUMENT_CHANGE();
  xassert(!m_listening);
  m_editor->addObserver(this);
  m_listening = true;
}


// General goal for dealing with inserted lines:  The cursor in the
// nonfocus window should not change its vertical location within the
// window (# of pixels from top window edge), and should remain on the
// same line (sequence of chars).  See doc/test-plan.txt, test
// "Multiple window simultaneous edit".

void EditorWidget::observeInsertLine(TextDocumentCore const &buf, int line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    TRACE("observe", "IGNORING: observeInsertLine line=" << line);
    return;
  }
  TRACE("observe", "observeInsertLine line=" << line);
  INITIATING_DOCUMENT_CHANGE();

  // Normally, we try to keep the cursor stationary in the window (as
  // explained above).  But for a process document, I instead want it to
  // work more like the user is typing text, so we will just scroll to
  // keep the cursor in view.
  bool keepCursorStationary =
    (m_editor->documentProcessStatus() != DPS_RUNNING);

  // Internally inside HE_text::insert(), the routine that actually
  // inserts text, inserting "line N" works by removing the text on
  // line N, inserting a new line N+1, then putting that text back on
  // line N+1.  It's sort of weird, and calls into question how much
  // observers ought to know about the mechanism.  But for now at least
  // I will compensate here by changing the line number to match what I
  // think of as the conceptually inserted line.
  line--;

  if (line <= m_editor->cursor().m_line) {
    m_editor->moveCursorBy(+1, 0);
    if (keepCursorStationary) {
      m_editor->moveFirstVisibleBy(+1, 0);
    }
    else {
      m_editor->scrollToCursor();
    }
  }

  if (m_editor->markActive() && line <= m_editor->mark().m_line) {
    m_editor->moveMarkBy(+1, 0);
  }

  redraw();
  GENERIC_CATCH_END
}

void EditorWidget::observeDeleteLine(TextDocumentCore const &buf, int line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    TRACE("observe", "IGNORING: observeDeleteLine line=" << line);
    return;
  }
  TRACE("observe", "observeDeleteLine line=" << line);
  INITIATING_DOCUMENT_CHANGE();

  if (line < m_editor->cursor().m_line) {
    m_editor->moveCursorBy(-1, 0);
    m_editor->moveFirstVisibleBy(-1, 0);
  }

  if (m_editor->markActive() && line < m_editor->mark().m_line) {
    m_editor->moveMarkBy(-1, 0);
  }

  redraw();
  GENERIC_CATCH_END
}


// For inserted characters, I don't do anything special, so
// the cursor says in the same column of text.

void EditorWidget::observeInsertText(TextDocumentCore const &, TextMCoord, char const *, int) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redraw();
  GENERIC_CATCH_END
}

void EditorWidget::observeDeleteText(TextDocumentCore const &, TextMCoord, int) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redraw();
  GENERIC_CATCH_END
}

void EditorWidget::observeTotalChange(TextDocumentCore const &buf) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redraw();
  GENERIC_CATCH_END
}

void EditorWidget::observeUnsavedChangesChange(TextDocument const *doc) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redraw();
  GENERIC_CATCH_END
}


void EditorWidget::rescuedKeyPressEvent(QKeyEvent *k)
{
  this->keyPressEvent(k);
}


bool EditorWidget::eventFilter(QObject *watched, QEvent *event) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Within the editor window, I do not use Tab for input focus changes,
  // but the existence of other focusable controls (when the Search and
  // Replace panel is open) causes Tab to be treated as such unless I
  // use an event filter.
  if (watched == this && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_Tab ||
        keyEvent->key() == Qt::Key_Backtab) {
      TRACE("EditorWidget", "Rescuing Tab press");
      rescuedKeyPressEvent(keyEvent);
      return true;       // no further processing
    }
  }

  return false;

  GENERIC_CATCH_END_RET(false)
}


// This is not called before every possible editing operation, since
// that would add considerable clutter, just the most common.  A
// missing edit safety check merely delays the user getting the
// warning until they try to save.
//
// There is a danger in scattering the warnings too widely because I
// might end up prompting in an inappropriate context, so currently
// the warnings are only in places where I can clearly see that the
// user just initiated an edit action and it is therefore safe to
// cancel it.
bool EditorWidget::editSafetyCheck()
{
  if (m_editor->isReadOnly() && !this->promptOverrideReadOnly()) {
    // Document is still read-only, user does not want to override.
    return false;
  }

  if (m_editor->unsavedChanges()) {
    // We already have unsaved changes, so assume that the safety
    // check has already passed or its warning dismissed.  (I do not
    // want to hit the disk for every edit operation.)
    return true;
  }

  if (!m_editor->m_namedDoc->m_modifiedOnDisk) {
    // No concurrent changes, safe to go ahead.
    return true;
  }

  // Prompt the user.
  QMessageBox box(this);
  box.setObjectName("editSafetyCheck_box");
  box.setWindowTitle("File Changed");
  box.setText(toQString(stringb(
    "The document " << m_editor->m_namedDoc->documentName() << " has changed on disk.  "
    "Do you want to proceed with editing the in-memory contents anyway, "
    "overwriting the on-disk changes when you later save?")));
  box.addButton(QMessageBox::Yes);
  box.addButton(QMessageBox::Cancel);
  int ret = box.exec();
  if (ret == QMessageBox::Yes) {
    // Go ahead with the edit.  This will cause us to have unsaved
    // changes, thus bypassing further warnings during editing, but
    // there will still be a warning before saving.
    return true;
  }
  else {
    // Cancel the edit.
    return false;
  }
}


void EditorWidget::command(std::unique_ptr<EditorCommand> cmd)
{
  TRACE("editor-command", "command: " << toGDValue(*cmd).asString());

  innerCommand(cmd.get());

  editorGlobal()->recordCommand(std::move(cmd));
}


void EditorWidget::innerCommand(EditorCommand const *cmd)
{
  // As this is where we act on the command to make a change, suppress
  // notifications here that might be caused by the change.
  INITIATING_DOCUMENT_CHANGE();

  ASTSWITCHC(EditorCommand, cmd) {
    ASTCASEC1(EC_Cut) {
      if (this->selectEnabled()) {
        setClipboard(m_editor->clipboardCut());
        this->redraw();
      }
    }

    ASTNEXTC1(EC_Copy) {
      if (this->selectEnabled()) {
        setClipboard(m_editor->clipboardCopy());
        this->redraw();
      }
    }

    ASTNEXTC1(EC_Paste) {
      QClipboard *cb = QApplication::clipboard();
      QString text;

      // Try reading the X selection first.  Generally this seems to reflect
      // the "more recent" deliberate clipboard interaction.
      if (cb->supportsSelection()) {
        text = cb->text(QClipboard::Selection);
      }

      // Then the regular clipboard.
      if (text.isEmpty()) {
        text = cb->text(QClipboard::Clipboard);
      }

      // Previously, I had a check here for empty `text`, and a warning
      // dialog.  But I want the processing of command objects to not
      // rely on being interactive, and the warning served little real
      // purpose, so I removed it.

      QByteArray utf8(text.toUtf8());
      m_editor->clipboardPaste(utf8.constData(), utf8.length());
      this->redraw();
    }

    ASTNEXTC1(EC_KillLine) {
      if (!selectEnabled()) {
        m_editor->selectCursorLine();
      }
      setClipboard(m_editor->clipboardCut());
      this->redraw();
    }

    ASTNEXTC1(EC_SelectEntireFile) {
      m_editor->selectEntireFile();
      this->redraw();
    }

    ASTNEXTC(EC_CursorToEndOfNextLine, ec) {
      m_editor->turnSelection(ec->m_select);
      int line = m_editor->cursor().m_line;
      m_editor->setCursor(m_editor->lineEndLCoord(line+1));
      scrollToCursor();
    }

    // This case is currently not called by anything.  I created it for
    // consistency with the other "EC_MoveFirstVisible..." commands.
    ASTNEXTC(EC_MoveFirstVisibleBy, ec) {
      m_editor->moveFirstVisibleBy(
        ec->m_deltaLine, ec->m_deltaColumn);
      redraw();
    }

    ASTNEXTC(EC_MoveFirstVisibleAndCursor, ec) {
      m_editor->moveFirstVisibleAndCursor(
        ec->m_deltaLine, ec->m_deltaColumn);
      redraw();
    }

    ASTNEXTC(EC_MoveFirstVisibleConfineCursor, ec) {
      m_editor->moveFirstVisibleConfineCursor(
        ec->m_deltaLine, ec->m_deltaColumn);
      redraw();
    }

    ASTNEXTC(EC_MoveCursorByCell, ec) {
      m_editor->turnSelection(ec->m_select);
      m_editor->moveCursorBy(ec->m_deltaLine, ec->m_deltaColumn);
      scrollToCursor();
    }

    ASTNEXTC(EC_MoveCursorByPage, ec) {
      m_editor->turnSelection(ec->m_select);
      m_editor->moveFirstVisibleAndCursor(ec->m_sign * this->visLines(), 0);
      redraw();
    }

    ASTNEXTC(EC_MoveCursorToLineExtremum, ec) {
      m_editor->turnSelection(ec->m_select);
      if (ec->m_start) {
        m_editor->setCursorColumn(0);
      }
      else {
        m_editor->setCursorColumn(m_editor->cursorLineLengthColumns());
      }
      scrollToCursor();
    }

    ASTNEXTC(EC_MoveCursorToFileExtremum, ec) {
      m_editor->turnSelection(ec->m_select);
      if (ec->m_start) {
        m_editor->moveCursorToTop();
      }
      else {
        m_editor->moveCursorToBottom();
      }
      redraw();
    }

    ASTNEXTC1(EC_DeleteKeyFunction) {
      m_editor->deleteKeyFunction();
      redraw();
    }

    ASTNEXTC1(EC_DeleteMenuFunction) {
      if (selectEnabled()) {
        m_editor->deleteSelection();
        redraw();
      }
    }

    ASTNEXTC1(EC_BackspaceFunction) {
      m_editor->backspaceFunction();
      redraw();
    }

    ASTNEXTC1(EC_CenterVisibleOnCursorLine) {
      m_editor->centerVisibleOnCursorLine();
      redraw();
    }

    ASTNEXTC(EC_BlockIndent, ec) {
      if (m_editor->blockIndent(ec->m_amt)) {
        redraw();
      }
    }

    ASTNEXTC1(EC_InsertNewlineAutoIndent) {
      m_editor->insertNewlineAutoIndent();
      redraw();
    }

    ASTNEXTC(EC_InsertString, ec) {
      m_editor->insertString(ec->m_text);
      this->redraw();
    }

    ASTENDCASECD
  }
}


void EditorWidget::runMacro(std::string const &name)
{
  EditorCommandVector commands = editorSettings().getMacro(name);
  for (auto const &cmdptr : commands) {
    innerCommand(cmdptr.get());
  }
}


bool EditorWidget::promptOverrideReadOnly()
{
  QMessageBox box(this);
  box.setObjectName("promptOverrideReadOnly_box");
  box.setWindowTitle("Read-only Document");
  box.setText(toQString(stringb(
    "The document " << m_editor->m_namedDoc->documentName() <<
    " is marked read-only.  Do you want to clear the read-only "
    "flag and edit it anyway?")));
  box.addButton(QMessageBox::Yes);
  box.addButton(QMessageBox::Cancel);
  int ret = box.exec();
  if (ret == QMessageBox::Yes) {
    m_editor->setReadOnly(false);

    // Go ahead with the edit.
    return true;
  }
  else {
    return false;
  }
}


bool EditorWidget::ignoringChangeNotifications() const
{
  return s_ignoreTextDocumentNotificationsGlobally ||
         m_ignoreTextDocumentNotifications;
}


int EditorWidget::getBaselineYCoordWithinLine() const
{
  // The baseline is the lowest pixel in the ascender region.
  return m_fontAscent - 1;
}


int EditorWidget::getFullLineHeight() const
{
  return m_fontHeight + m_interLineSpace;
}


string EditorWidget::eventReplayQuery(string const &state)
{
  if (state == "firstVisible") {
    return stringb(m_editor->firstVisible());
  }
  else if (state == "lastVisible") {
    return stringb(m_editor->lastVisible());
  }
  else if (state == "documentProcessState") {
    return toString(m_editor->documentProcessStatus());
  }
  else if (state == "hasUnsavedChanges") {
    return string(m_editor->unsavedChanges()? "true" : "false");
  }
  else if (state == "resourceName") {
    return m_editor->m_namedDoc->resourceName();
  }
  else if (state == "documentFileName") {
    // Strip path info.
    return SMFileUtil().splitPathBase(m_editor->m_namedDoc->filename());
  }
  else if (state == "documentText") {
    return m_editor->getTextForLRangeString(m_editor->documentLRange());
  }
  else if (state == "selfCheck") {
    // Just invoke self check, throwing if it fails.  The returned
    // string is not meaningful.
    this->selfCheck();
    return "";
  }
  else {
    return EventReplayQueryable::eventReplayQuery(state);
  }
}


QImage EditorWidget::eventReplayImage(string const &what)
{
  if (what == "screenshot") {
    return this->getScreenshot();
  }
  else {
    return EventReplayQueryable::eventReplayImage(what);
  }
}


bool EditorWidget::wantResizeEventsRecorded()
{
  return true;
}


// EOF
