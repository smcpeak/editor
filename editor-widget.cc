// editor-widget.cc
// code for editor-widget.h

#include "editor-widget.h"                       // this module

// editor
#include "column-difference.h"                   // ColumnDifference
#include "completions-dialog.h"                  // CompletionsDialog
#include "debug-values.h"                        // DEBUG_VALUES
#include "diagnostic-details-dialog.h"           // DiagnosticDetailsDialog
#include "diagnostic-element.h"                  // DiagnosticElement
#include "doc-type-detect.h"                     // languageName
#include "doc-type.h"                            // DocumentType
#include "editor-command.ast.gen.h"              // EditorCommand
#include "editor-navigation-options.h"           // EditorNavigationOptions
#include "editor-global.h"                       // EditorGlobal
#include "editor-window.h"                       // EditorWindow
#include "fail-reason-opt.h"                     // FailReasonOpt
#include "host-file-line.h"                      // HostFileLine
#include "host-file-olb.h"                       // HostFile_OptLineByte
#include "json-rpc-reply.h"                      // JSON_RPC_Reply
#include "line-number.h"                         // LineNumber
#include "list-choice-dialog.h"                  // ListChoiceDialog
#include "lsp-data.h"                            // LSP_LocationSequence
#include "lsp-conv.h"                            // toMCoordRange, toLSP_VersionNumber, lspLanguageIdForDTOpt
#include "lsp-client-manager.h"                  // LSPClientManager
#include "lsp-client.h"                          // LSPClient::notify_textDocument_didOpen, etc.
#include "lsp-symbol-request-kind.h"             // LSPSymbolRequestKind
#include "lsp-version-number.h"                  // LSP_VersionNumber
#include "nearby-file.h"                         // getNearbyFilename
#include "styledb.h"                             // StyleDB, TextCategoryAndStyle
#include "range-text-repl.h"                     // RangeTextReplacement
#include "td-diagnostics.h"                      // TextDocumentDiagnostics
#include "tdd-proposed-fix.h"                    // TDD_ProposedFix
#include "textcategory.h"                        // LineCategoryAOAs, etc.
#include "uri-util.h"                            // getFileURIPath, makeFileURI
#include "vfs-query-sync.h"                      // VFS_QuerySync
#include "waiting-counter.h"                     // IncDecWaitingCounter

// smqtutil
#include "smqtutil/courB24_ISO8859_1.bdf.gen.h"  // bdfFontData_courB24_ISO8859_1
#include "smqtutil/courO24_ISO8859_1.bdf.gen.h"  // bdfFontData_courO24_ISO8859_1
#include "smqtutil/courR24_ISO8859_1.bdf.gen.h"  // bdfFontData_courR24_ISO8859_1
#include "smqtutil/editor14b.bdf.gen.h"          // bdfFontData_editor14b
#include "smqtutil/editor14i.bdf.gen.h"          // bdfFontData_editor14i
#include "smqtutil/editor14r.bdf.gen.h"          // bdfFontData_editor14r
#include "smqtutil/gdvalue-qt.h"                 // toGDValue(QMouseEvent)
#include "smqtutil/minihex6.bdf.gen.h"           // bdfFontData_minihex6
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtbdffont.h"                  // QtBDFFont, drawHexQuad
#include "smqtutil/qtguiutil.h"                  // keysString(QKeyEvent), QPainterSaveRestore, showRaiseAndActivateWindow
#include "smqtutil/qtutil.h"                     // toString(QString), SET_QOBJECT_NAME, toQString
#include "smqtutil/sync-wait.h"                  // synchronouslyWaitUntil, SynchronousWaiter

// smbase
#include "smbase/array.h"                        // Array
#include "smbase/bdffont.h"                      // BDFFont
#include "smbase/c-string-reader.h"              // decodeCStringEscapesToString
#include "smbase/dev-warning.h"                  // DEV_WARNING
#include "smbase/exc.h"                          // GENERIC_CATCH_BEGIN/END, smbase::{XBase, XMessage, xmessage}, EXN_CONTEXT
#include "smbase/gdvalue-optional.h"             // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-subst-transform.h"      // gdv::substitutionTransformGDValue
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::toGDValue
#include "smbase/list-util.h"                    // smbase::listAtC
#include "smbase/nonport.h"                      // getMilliseconds
#include "smbase/objcount.h"                     // CHECK_OBJECT_COUNT
#include "smbase/save-restore.h"                 // SetRestore
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/stringb.h"                      // stringb
#include "smbase/strutil.h"                      // dirname
#include "smbase/xassert.h"                      // xassert
#include "smbase/xoverflow.h"                    // smbase::XNumericConversion

// Qt
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QProgressDialog>
#include <QRect>

// libc++
#include <algorithm>                             // std::min
#include <functional>                            // std::function
#include <memory>                                // std::shared_ptr
#include <optional>                              // std::{nullopt, optional}
#include <string>                                // std::string
#include <string_view>                           // std::string_view
#include <utility>                               // std::move

using namespace gdv;
using namespace smbase;


// Trace levels here:
//
//   1. Operations on files.
//
//   2. Keystrokes, mouse clicks.
//
//   3. Repaint, mouse drags.
//
INIT_TRACE("editor-widget");


// The basic rule for using this is it should be present in any function
// that calls a non-const method of TextDocumentEditor.  This includes
// cursor changes, even though those currently do not have an associated
// notification event, since they might have one in the future.  In
// order to not have redundant code, I mostly only use this in those
// functions that make direct calls.  For consistency, I even do this
// for the destructor, or when I know I am not listening, since there is
// essentially no cost to doing it.
#define INITIATING_DOCUMENT_CHANGE()                      \
  SetRestore<bool> ignoreNotificationsRestorer(           \
    m_ignoreTextDocumentNotifications, true) /* user ; */


// Invoke `command`, passing a `unique_ptr` to a newly created command
// object of type `CommandType`, and forwarding arguments as needed.
#define COMMAND_MU(CommandType, ...) \
  command(std::make_unique<CommandType>(__VA_ARGS__)) /* user ; */


// Run a command if the edit safety check passes.  Generally this should
// be used whenever the command could change the document contents.
#define EDIT_COMMAND_MU(CommandType, ...) \
  if (editSafetyCheck()) {                \
    COMMAND_MU(CommandType, __VA_ARGS__); \
  }


// Distance below the baseline to draw an underline.
int const UNDERLINE_OFFSET = 2;

// Number of lines or columns to move for Ctrl+Shift+<arrow>.
int const CTRL_SHIFT_DISTANCE = 10;

// Desired line/column gap between search match and screen edge.
int const SAR_SCROLL_GAP = 10;


// ---------------------- EditorWidget --------------------------------
int EditorWidget::s_objectCount = 0;

bool EditorWidget::s_ignoreTextDocumentNotificationsGlobally = false;

CHECK_OBJECT_COUNT(EditorWidget);


EditorWidget::EditorWidget(NamedTextDocument *tdf,
                           EditorWindow *editorWindow)
  : QWidget(editorWindow),
    m_editorWindow(editorWindow),
    m_infoBox(NULL),
    m_matchesAboveLabel(NULL),
    m_matchesBelowLabel(NULL),
    m_editorList(),
    m_editor(NULL),
    m_fileStatusRequestID(0),
    m_fileStatusRequestEditor(),
    m_hitText(""),
    m_hitTextFlags(TextSearch::SS_CASE_INSENSITIVE),
    m_textSearch(),
    m_topMargin(1),
    m_leftMargin(1),
    m_interLineSpace(0),
    m_cursorColor(0xFF, 0xFF, 0xFF),       // white
    m_fontSet(),
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

  m_textSearch.reset(new TextSearch(m_editor->getDocumentCore()));
  this->setTextSearchParameters();

  editorGlobal()->addDocumentListObserver(this);

  setFontsFromEditorGlobal();

  setCursor(Qt::IBeamCursor);

  // required to accept focus
  setFocusPolicy(Qt::StrongFocus);

  // This causes 'this->eventFilter()' to be invoked when 'this'
  // receives events.  It is needed to ensure Tab gets seen by the
  // editor widget.
  installEventFilter(this);

  QObject::connect(vfsConnections(), &VFS_Connections::signal_vfsReplyAvailable,
                   this, &EditorWidget::on_vfsReplyAvailable);
  QObject::connect(vfsConnections(), &VFS_Connections::signal_vfsFailed,
                   this, &EditorWidget::on_vfsConnectionFailed);

  selfCheck();

  EditorWidget::s_objectCount++;
}


EditorWidget::~EditorWidget()
{
  EditorWidget::s_objectCount--;

  this->stopListening();

  editorGlobal()->removeDocumentListObserver(this);
  editorGlobal()->removeRecentEditorWidget(this);

  m_textSearch.reset();

  QObject::disconnect(vfsConnections(), nullptr, this, nullptr);
  cancelFileStatusRequestIfAny();

  // Do this explicitly just for clarity, but the automatic destruction
  // should also work.
  m_editor = NULL;
  m_editorList.deleteAll();

  m_fontSet.deleteAll();

  // Explicit for clarity.
  m_editorWindow = nullptr;
}


void EditorWidget::selfCheck() const
{
  // Check that `editor` is among `m_editors` and that the files in
  // `m_editors` are a subset of those known to `editorGlobal()`.
  bool foundEditor = false;
  FOREACH_OBJLIST(NamedTextDocumentEditor, m_editorList, iter) {
    NamedTextDocumentEditor const *tdfe = iter.data();
    if (m_editor == tdfe) {
      foundEditor = true;
    }
    tdfe->selfCheck();
    xassert(editorGlobal()->hasDocumentFile(tdfe->m_namedDoc));
  }
  xassert(foundEditor);

  // There should never be more m_editors than fileDocuments.
  xassert(editorGlobal()->numDocuments() >= m_editorList.count());

  xassert((m_fileStatusRequestID == 0) ==
          (m_fileStatusRequestEditor == nullptr));
  xassert(m_fileStatusRequestEditor == nullptr ||
          m_fileStatusRequestEditor == m_editor);

  // Check that 'm_listening' agrees with the document's observer list.
  xassert(m_listening == m_editor->hasObserver(this));

  // And, at this point, we should always be listening.
  xassert(m_listening);

  // Check 'm_textSearch'.
  xassert(m_textSearch != nullptr);
  xassert(m_hitText == m_textSearch->searchString());
  xassert(m_hitTextFlags == m_textSearch->searchStringFlags());
  xassert(m_textSearch->document() == m_editor->getDocumentCore());
  m_textSearch->selfCheck();

  m_fontSet.selfCheck();
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


static std::unique_ptr<BDFFont> makeBDFFont(
  char const *bdfData, char const *context)
{
  EXN_CONTEXT(context);

  std::unique_ptr<BDFFont> ret(new BDFFont);
  parseBDFString(*ret, bdfData);
  return ret;
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
  EditorFontSet::FontVariantToBDFFont bdfFonts;
  bdfFonts.at(FV_NORMAL) = makeBDFFont(normal, "normal font");
  bdfFonts.at(FV_ITALIC) = makeBDFFont(italic, "italic font");
  bdfFonts.at(FV_BOLD) = makeBDFFont(bold, "bold font");

  // Using one fixed global style mapping.
  StyleDB *styleDB = StyleDB::instance();

  // Font for missing glyphs.
  std::unique_ptr<BDFFont> minihexBDFFont(
    makeBDFFont(bdfFontData_minihex6, "minihex font"));

  // Build the complete set of new fonts.
  {
    EditorFontSet newFonts(
      styleDB,
      bdfFonts,
      *minihexBDFFont,
      m_cursorColor);

    // Substitute the new for the old.
    m_fontSet.swapWith(newFonts);
  }

  // calculate metrics
  QRect const bbox =
    m_fontSet.forCatAOAC(TC_NORMAL)->getAllCharsBBox();
  m_fontAscent = -bbox.top();
  m_fontDescent = bbox.bottom() + 1;
  m_fontHeight = m_fontAscent + m_fontDescent;
  xassert(m_fontHeight == bbox.height());    // check my assumptions
  m_fontWidth = bbox.width();
}


void EditorWidget::setDocumentFile(NamedTextDocument *file)
{
  this->stopListening();
  this->cancelFileStatusRequestIfAny();

  m_editor = this->getOrMakeEditor(file);

  if (recomputeLastVisible()) {
    // If `file` was most recently shown with the cursor at the bottom
    // of the screen and the search-and-replace bar *not* shown, but now
    // the bar *is* shown, then the cursor will be just barely
    // offscreen, hidden by the bar.  That's annoying because I can't
    // immediately tell where it is or that it is nearby.  So, if the
    // cursor is near the bottom edge, scroll a little so it becomes
    // visible.
    m_editor->scrollToCursorIfBarelyOffscreen(
      LineDifference(3) /*howFar*/, 2 /*edgeGap*/);
  }

  // This deallocates the old 'TextSearch'.
  m_textSearch.reset(new TextSearch(m_editor->getDocumentCore()));
  this->setTextSearchParameters();

  // Move the chosen file to the top of the document list since it is
  // now the most recently used.
  editorGlobal()->makeDocumentTopmost(file);

  this->startListening();

  // Draw the current contents.
  this->redrawAfterContentChange();

  // Then, issue a request to refresh those contents.
  this->requestFileStatus();
}


NamedTextDocumentEditor *EditorWidget::getOrMakeEditor(
  NamedTextDocument *file_)
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
  bool hasView = editorGlobal()->getInitialViewForFile(file, view);

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

  TRACE1("requestFileStatus: VFS request id=" << m_fileStatusRequestID);
}


void EditorWidget::cancelFileStatusRequestIfAny()
{
  if (m_fileStatusRequestID) {
    TRACE1("cancelFileStatusRequestIfAny: VFS id=" << m_fileStatusRequestID);
    vfsConnections()->cancelRequest(m_fileStatusRequestID);
    m_fileStatusRequestID = 0;
    m_fileStatusRequestEditor = nullptr;
  }
}


void EditorWidget::on_vfsReplyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (requestID == m_fileStatusRequestID) {
    TRACE1("on_vfsReplyAvailable: id=" << requestID);

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
        TRACE1(
          "Document modTime " << getDocument()->m_lastFileTimestamp <<
          " differs from reply modTime " << reply->m_fileModificationTime <<
          ", marking as modified on disk.");
        getDocument()->m_modifiedOnDisk = true;
        this->redraw();
      }
      else {
        TRACE2(
          "Document modTime " << getDocument()->m_lastFileTimestamp <<
          " is same as reply modTime " << reply->m_fileModificationTime <<
          ", NOT marking as modified on disk.");
      }
    }
  }

  GENERIC_CATCH_END
}


void EditorWidget::on_vfsConnectionFailed(
  HostName hostName, string reason) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_vfsConnectionFailed: host=" << hostName <<
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


void EditorWidget::openDiagnosticOrFileAtCursor(
  EditorNavigationOptions opts)
{
  if (FailReasonOpt msg = lspShowDiagnosticAtCursor(opts);
      !msg.has_value()) {
    // We successfully showed the diagnostic messge, so do not proceed
    // with trying to open a file.
    return;
  }

  TextLCoord cursorLC = m_editor->cursor();
  string lineText = m_editor->getWholeLineString(cursorLC.m_line);

  // We will look for the file whose name is under the cursor in any
  // directory where we already have an open file, starting with the
  // directory where the current file is.
  std::vector<HostAndResourceName> prefixes;
  prefixes.push_back(getDocumentDirectoryHarn());

  // Then, look in directories of other files, with the most recently
  // used files considered first.
  editorGlobal()->getUniqueDocumentDirectories(prefixes);

  SynchronousWaiter waiter(this);
  VFS_QuerySync querySync(vfsConnections(), waiter);

  // Convert to model coordinates in order to get a byte index into
  // `lineText`.
  TextMCoord cursorMC = m_editor->toMCoord(cursorLC);

  std::optional<HostFile_OptLineByte> hostFileAndLine =
    getNearbyFilename(querySync, prefixes,
                      lineText, cursorMC.m_byteIndex);

  if (!hostFileAndLine) {
    // TODO: Show the list of candidate prefixes.
    complain("Did not find any files to open at cursor.");
  }
  else {
    // Choose which widget will navigate.
    EditorWidget *ew = editorGlobal()->selectEditorWidget(this, opts);

    // Go to the indicated file and line.
    ew->doOpenOrSwitchToFileAtLineOpt(*hostFileAndLine);
  }
}


void EditorWidget::doOpenOrSwitchToFileAtLineOpt(
  HostFile_OptLineByte const &hostFileAndLine)
{
  // This should be sent on a Qt::QueuedConnection, meaning the slot
  // will be invoked later, once the current event is done processing.
  // That is important because right now we have an open
  // TDE_HistoryGrouper, but opening a new file might close the one we
  // are currently looking at if it is untitled, which will cause the
  // RCSerf infrastructure to abort just before memory corruption would
  // have resulted.
  Q_EMIT signal_openOrSwitchToFileAtLineOpt(
    hostFileAndLine, false /*promptIfNotFound*/);
}


void EditorWidget::makeCurrentDocumentTopmost()
{
  editorGlobal()->makeDocumentTopmost(getDocument());
}


void EditorWidget::namedTextDocumentRemoved(
  NamedTextDocumentList const * /*documentList*/,
  NamedTextDocument *file) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Change files if that was the one we were editing.  Do this before
  // destroying any editors.
  if (m_editor->m_namedDoc == file) {
    this->setDocumentFile(editorGlobal()->getDocumentByIndex(0));
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
  NamedTextDocumentList const * /*documentList*/,
  NamedTextDocument *file,
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


void EditorWidget::redrawAfterContentChange()
{
  Q_EMIT signal_contentChange();
  redraw();
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


void EditorWidget::complain(std::string_view msg) const
{
  editorWindow()->complain(msg);
}


void EditorWidget::inform(std::string_view msg) const
{
  editorWindow()->inform(msg);
}


string EditorWidget::applyCommandSubstitutions(string const &orig) const
{
  return m_editor->applyCommandSubstitutions(orig);
}


// Compute and broadcast match status indicator.
void EditorWidget::emitSearchStatusIndicator()
{
  if (!m_textSearch->searchStringIsValid()) {
    // This is a bit crude as an error explanation, but it seems
    // adequate for an initial implementation.
    std::ostringstream sb;
    sb << "Err @ " << m_textSearch->searchStringErrorOffset();
    TRACE2("emitSearchStatusIndicator: " << sb.str());
    Q_EMIT signal_searchStatusIndicator(toQString(sb.str()));
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

  std::ostringstream sb;
  sb << matchesLT << " [";
  if (matchesSelected) {
    sb << 'x';
  }
  sb << "] " << matchesGTE;
  if (m_textSearch->hasIncompleteMatches()) {
    sb << '+';
  }

  TRACE2("emitSearchStatusIndicator: " << sb.str());
  Q_EMIT signal_searchStatusIndicator(toQString(sb.str()));
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
  LineDifference deltaLine, ColumnDifference deltaCol)
{
  COMMAND_MU(EC_MoveFirstVisibleAndCursor, deltaLine, deltaCol);
}


bool EditorWidget::recomputeLastVisible()
{
  int h = this->height();
  int w = this->width();

  if (m_fontHeight && m_fontWidth) {
    INITIATING_DOCUMENT_CHANGE();

    // calculate viewport size
    m_editor->setVisibleSize(
      (h - m_topMargin) / this->lineHeight(),
      ColumnCount((w - m_leftMargin) / m_fontWidth));

    return true;
  }
  else {
    // font info not set, leave them alone
    return false;
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
    TRACE3("updateFrame: rect=" << rect);
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
  LineCategoryAOAs modelCategories(TC_NORMAL);

  // The style info, but expressed in layout coordinates.  For each
  // line, we first compute 'modelCategories', then 'layoutCategories'
  // is computed from the former.
  LineCategoryAOAs layoutCategories(TC_NORMAL);

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
  ColumnCount const visibleCols = this->visColsPlusPartial();
  ColumnIndex const firstCol = this->firstVisibleCol();
  LineIndex const firstLine = this->firstVisibleLine();

  // I think it might be useful to support negative values for these
  // variables, but the code below is not prepared to deal with such
  // values at this time
  xassert(firstCol >= 0);

  // another sanity check
  xassert(lineHeight() > 0);

  // Buffer that will be used for each visible line of text.
  ArrayStack<char> visibleText(visibleCols.get());

  // Get region of selected text.
  TextLCoordRange selRange = m_editor->getSelectLayoutRange();

  // Paint the window, one line at a time.  Both 'line' and 'y' act
  // as loop control variables.
  for (LineIndex line = firstLine;
       y < this->height();
       ++line, y += fullLineHeight)
  {
    // ---- compute style segments ----
    // Number of columns from this line that are visible.
    ColumnCount visibleLineCols(0);

    // nominally the entire line is normal text
    modelCategories.clear(TC_NORMAL);
    layoutCategories.clear(TC_NORMAL);

    // This is 1 if we will behave as though a newline character is
    // at the end of this line, 0 otherwise.
    ColumnDifference newlineAdjust(0);
    if (m_visibleWhitespace && line < m_editor->numLines().pred()) {
      ++newlineAdjust;
    }

    // True if the cursor is on `line`.
    bool const cursorOnCurrentLine =
      (line == m_editor->cursor().m_line);

    // Number of cells in the line, excluding newline.
    ColumnCount const lineLengthColumns =
      m_editor->lineLengthColumns(line);

    // How many columns of trailing whitespace does this line have?
    ColumnCount const lineTrailingWhitespaceCols =
      cursorOnCurrentLine?
        ColumnCount(0) :     // Don't highlight trailing WS on the cursor line.
        m_editor->countTrailingSpacesTabsColumns(line);

    // Column number within the visible window of the first trailing
    // whitespace character.  All characters in the line at or beyond
    // this value will be printed with a different background color.
    ColumnDifference const startOfTrailingWhitespaceVisibleCol =
      lineLengthColumns - lineTrailingWhitespaceCols - firstCol;

    // Number of columns with glyphs on this line, including possible
    // synthesized newline for 'visibleWhitespace'.  This value is
    // independent of the window size or scroll position.
    ColumnCount const lineGlyphColumns =
      lineLengthColumns + newlineAdjust;

    // fill with text from the file
    visibleText.clear();
    if (line < m_editor->numLines()) {
      if (firstCol < lineGlyphColumns) {
        // First get the text without any extra newline.
        ColumnCount const visibleLengthColumns(lineLengthColumns - firstCol);
        ColumnCount const amt = std::min(visibleLengthColumns, visibleCols);
        m_editor->getLineLayout(TextLCoord(line, firstCol), visibleText, amt);
        visibleLineCols = amt;

        // Now possibly add the newline.
        if (visibleLineCols < visibleCols && newlineAdjust != 0) {
          visibleText.push('\n');
          ++visibleLineCols;
        }
      }

      // Apply syntax highlighting.
      if (m_editor->m_namedDoc->highlighter()) {
        m_editor->m_namedDoc->highlighter()
          ->highlightTDE(m_editor, line, /*OUT*/ modelCategories);
        m_editor->modelToLayoutSpans(line,
          /*OUT*/ layoutCategories, /*IN*/ modelCategories);
      }

      // Show search hits.
      this->addSearchMatchesToLineCategories(layoutCategories, line);
    }
    xassert(visibleLineCols <= visibleCols);
    xassert(visibleText.length() == visibleLineCols);

    // Fill the remainder of 'visibleText' with spaces.  These
    // characters will only be used if there is style information out
    // beyond the actual line character data.
    {
      ColumnCount remainderLen(visibleCols - visibleLineCols);
      memset(visibleText.ptrToPushedMultipleAlt(remainderLen.get()),
             ' ', remainderLen.get());
    }
    xassert(visibleText.length() == visibleCols);

    // incorporate effect of selection
    if (this->selectEnabled() &&
        selRange.m_start.m_line <= line && line <= selRange.m_end.m_line)
    {
      if (selRange.m_start.m_line < line && line < selRange.m_end.m_line) {
        // entire line is selected
        layoutCategories.overlay(0, 0 /*infinite*/,
          TOA_SELECTION);
      }
      else if (selRange.m_start.m_line < line && line == selRange.m_end.m_line) {
        // Left half of line is selected.
        if (selRange.m_end.m_column) {
          layoutCategories.overlay(0, selRange.m_end.m_column.get(),
            TOA_SELECTION);
        }
      }
      else if (selRange.m_start.m_line == line && line < selRange.m_end.m_line) {
        // Right half of line is selected.
        layoutCategories.overlay(selRange.m_start.m_column.get(), 0 /*infinite*/,
          TOA_SELECTION);
      }
      else if (selRange.m_start.m_line == line && line == selRange.m_end.m_line) {
        // Middle part of line is selected.
        if (selRange.m_end.m_column != selRange.m_start.m_column) {
          layoutCategories.overlay(
            selRange.m_start.m_column.get(),
            (selRange.m_end.m_column - selRange.m_start.m_column).get(),
            TOA_SELECTION);
        }
      }
      else {
        xfailure("messed up my logic");
      }
    }

    // Iterator over line contents.  This is partially redundant with
    // what is in 'visibleText', but needed to handle glyphs that span
    // columns.  Perhaps I should remove 'visibleText' at some point.
    TextDocumentEditor::LineIterator lineIter(*m_editor, line);
    while (lineIter.has() && lineIter.columnOffset() < firstCol) {
      lineIter.advByte();
    }

    // Given that we have chosen how to render the line, storing that
    // information primarily into `visibleText` (chars to draw) and
    // `layoutCategories` (how to draw them), draw the line to `paint`.
    paintOneLine(
      paint,
      visibleLineCols,
      startOfTrailingWhitespaceVisibleCol,
      layoutCategories,
      visibleText,
      std::move(lineIter),
      /*INOUT*/ textCategoryAndStyle);

    drawDiagnosticBoxes(paint, line);

    // Draw the cursor on the line it is on.
    if (cursorOnCurrentLine) {
      drawCursorOnLine(
        paint,
        layoutCategories,
        visibleText,
        lineGlyphColumns);
    }

    drawSoftMarginIndicator(paint);

    // draw the line buffer to the window
    winPaint.drawPixmap(0,y, pixmap);    // draw it
  }

  // Also draw indicators of number of matches offscreen.
  this->drawOffscreenMatchIndicators(winPaint);
}


// In this code, we calculate layouts where each byte is one column.
static char const &at(ArrayStack<char> const &arr, ColumnIndex index)
{
  return arr[index.get()];
}


void EditorWidget::paintOneLine(
  // Painting target.
  QPainter &paint,

  // Number of columns from this line that are visible, including the
  // possible synthetic newline.  If this is less than
  // `visibleText.length()` (which is common), it means the line has
  // blank space before the right edge of the widget, and we will paint
  // that space with one large rectangle rather than however many
  // character cells.
  ColumnCount visibleLineCols,

  // Column number within the visible window of the first trailing
  // whitespace character.  All characters at or beyond this value will
  // be printed with a different background color.
  ColumnDifference startOfTrailingWhitespaceVisibleCol,

  // The styles to apply to the entire line of text (independent of the
  // window).  This function has to ignore whatever is outside the
  // current window area.
  LineCategoryAOAs const &layoutCategories,

  // Characters to draw, one per visible column within the window.
  // Blank space between EOL and the window edge is filled with spaces
  // (but that does not matter because of `visibleLineCols`).
  ArrayStack<char> const &visibleText,

  // Iterator over the bytes in the line, starting with the first that
  // is visible in the window.  This is used to adjust drawing for
  // characters that occupy multiple columns.
  //
  // TODO: This is a weird system.  It's mostly redundant with the
  // layout done to create `visibleText`, and the only effect is (I
  // think) on how trailing tab characters are drawn (which doesn't
  // really look right anyway).  I think I had in mind that this would
  // allow me to draw single glyphs that spanned columns, but if so,
  // that is unfinished.
  TextDocumentEditor::LineIterator &&lineIter,

  // Current text styling details, carried forward from line to line as
  // we work our way down the widget area.
  TextCategoryAndStyle /*INOUT*/ &textCategoryAndStyle)
{
  xassert(visibleLineCols <= visibleText.length());

  int const lineWidth = width();
  int const fullLineHeight = getFullLineHeight();
  ColumnCount const visibleCols = visColsPlusPartial();
  ColumnIndex const firstCol = firstVisibleCol();

  // Clear the left margin to the normal background color.
  textCategoryAndStyle.setDrawStyleIfNewCategory(paint, TC_NORMAL);
  paint.eraseRect(0,0, m_leftMargin, fullLineHeight);

  // Next category entry to use.
  LineCategoryAOAIter category(layoutCategories);
  category.advance(firstCol.get());

  // ---- render text+style segments -----
  // right edge of what has not been painted, relative to
  // the pixels in the pixmap
  int x = m_leftMargin;

  // Number of columns printed.
  ColumnCount printedCols(0);

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
      category.value());

    // compute how many characters to print in this segment
    ColumnCount len(category.runLength());
    if (category.runLength() == 0) {
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
      len = ColumnCount(visibleLineCols-printedCols);
    }
    len = std::min(len, ColumnCount(visibleCols-printedCols));
    xassert(len > 0);

    // The QtBDFFont package must be treated as if it draws
    // characters with transparency, even though the transparency
    // is only partial...
    paint.eraseRect(x,0, m_fontWidth*len, fullLineHeight);

    // The number of columns to draw for this segment is the smaller of
    // (a) segment length and (b) the number of columns left to print.
    // It could be negative, in which case the following loop will do
    // nothing.
    ColumnDifference const colsToDraw =
      std::min(ColumnDifference(len), visibleLineCols-printedCols);

    // draw text
    for (ColumnIndex i(0); i < colsToDraw; ++i) {
      if (lineIter.has()) {
        if (lineIter.columnOffset() > firstCol+printedCols+i) {
          // This column is part of a multicolumn glyph.  Do not
          // draw any glyph here.
          continue;
        }
        xassert(lineIter.columnOffset() == firstCol+printedCols+i);
        lineIter.advByte();
      }
      else if (at(visibleText, i+printedCols) != '\n') {
        // The only thing we should need to print beyond what is in
        // the line iterator is a newline, so skip drawing here.
        continue;
      }

      bool withinTrailingWhitespace =
        printedCols + i >= startOfTrailingWhitespaceVisibleCol;
      this->drawOneChar(paint,
                        textCategoryAndStyle.getFont(),
                        QPoint(x + m_fontWidth*i, baseline),
                        at(visibleText, i+printedCols),
                        withinTrailingWhitespace);

    } // character loop (within segment)

    if (textCategoryAndStyle.underlining()) {
      drawUnderline(paint, x, len);
    }

    // Advance to next category segment.
    x += m_fontWidth * len;
    printedCols += len;
    category.advance(len.get());

  } // segment loop
}


void EditorWidget::drawUnderline(
  QPainter &paint, int x, ColumnCount numCols)
{
  int const baseline = getBaselineYCoordWithinLine();

  // want to draw a line on top of where underscores would be; this
  // might not be consistent across fonts, so I might want to have
  // a user-specifiable underlining offset.. also, I don't want this
  // going into the next line, so truncate according to descent
  int ulBaseline = baseline + std::min(UNDERLINE_OFFSET, m_fontDescent);
  paint.drawLine(x, ulBaseline, x + m_fontWidth*numCols, ulBaseline);
}


std::optional<ColumnIndex> EditorWidget::byteIndexToLayoutColOpt(
  LineIndex line,
  std::optional<ByteIndex> byteIndex) const
{
  if (byteIndex) {
    return m_editor->toLCoord(TextMCoord(line, *byteIndex)).m_column;
  }
  else {
    return std::nullopt;
  }
}


void EditorWidget::drawDiagnosticBoxes(
  QPainter &paint,
  LineIndex line)
{
  // Does the document have any associated diagnostics?
  TextDocumentDiagnostics const *diagnostics =
    m_editor->m_namedDoc->getDiagnostics();
  if (!diagnostics) {
    return;
  }

  // Are there any diagnostics on this line?
  std::set<TextDocumentDiagnostics::LineEntry> entries =
    diagnostics->getLineEntries(line);
  if (entries.empty()) {
    return;
  }

  ColumnIndex const firstCol = firstVisibleCol();

  QPainterSaveRestore qpsr(paint);

  // For now, just draw using a fixed red color.
  paint.setPen(QColor(255, 0, 0));
  paint.setBrush(Qt::NoBrush);

  for (auto const &entry : entries) {
    std::optional<ColumnIndex> startCol =
      byteIndexToLayoutColOpt(line, entry.m_startByteIndex);
    std::optional<ColumnIndex> endCol =
      byteIndexToLayoutColOpt(line, entry.m_endByteIndex);

    int bottomY = m_fontHeight - 1;

    int leftX = 0;
    if (startCol) {
      if (endCol && (*startCol == *endCol)) {
        // For a collapsed span, draw a thin box at the left side of the
        // start column cell.
        int x = m_fontWidth * (*startCol - firstCol);
        paint.drawRect(x, 0, 2 /*w*/, bottomY /*h*/);
        continue;
      }

      // Left edge.
      leftX = m_fontWidth * (*startCol - firstCol);
      paint.drawLine(leftX, 0, leftX, bottomY);
    }

    int rightX;
    if (endCol) {
      // Right edge.
      rightX = m_fontWidth * (*endCol - firstCol) - 1;
      paint.drawLine(rightX, 0, rightX, bottomY);
    }
    else {
      rightX =
        m_fontWidth * (m_editor->lineLengthColumns(line) - firstCol);
    }

    // Top edge.
    paint.drawLine(leftX, 0, rightX, 0);

    // Bottom edge.
    paint.drawLine(leftX, bottomY, rightX, bottomY);
  }
}


void EditorWidget::drawCursorOnLine(
  QPainter &paint,
  LineCategoryAOAs const &layoutCategories,
  ArrayStack<char> const &visibleText,
  ColumnCount lineGlyphColumns)
{
  // just testing the mechanism that catches exceptions
  // raised while drawing
  //if (line == 5) {
  //  THROW(XBase("aiyee! sample exception!"));
  //}

  QPainterSaveRestore qpsr(paint);

  ColumnCount const visibleCols = visColsPlusPartial();

  ColumnIndex const firstCol = firstVisibleCol();
  ColumnIndex const cursorCol = m_editor->cursor().m_column;

  // 0-based cursor column relative to what is visible
  ColumnDifference const visibleCursorCol = cursorCol - firstCol;

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

    // The test above ensures this is non-negative.
    ColumnIndex visibleCursorColIndex(visibleCursorCol);

    // The character shown inside the box should use the same
    // font as if it were not inside the cursor box, to minimize
    // the visual disruption caused by the cursor's presence.
    //
    // Unfortunately, that leads to some code duplication with the
    // main painting code.
    TextCategoryAOA cursorCategory =
      layoutCategories.getCategoryAOAAt(cursorCol.get());
    FontVariant cursorFV = styleDB->getStyle(cursorCategory).variant;
    bool underlineCursor = false;
    if (cursorFV == FV_UNDERLINE) {
      cursorFV = FV_NORMAL;   // 'cursorFontForFV' does not map FV_UNDERLINE
      underlineCursor = true;
    }
    QtBDFFont *cursorFont = m_fontSet.forCursorForFV(cursorFV);

    paint.setBackground(cursorFont->getBgColor());
    paint.eraseRect(x,0, m_fontWidth, m_fontHeight);

    if (cursorCol < lineGlyphColumns) {
      // Drawing the block cursor overwrote the glyph, so we
      // have to draw it again.
      if (at(visibleText, visibleCursorColIndex) == ' ' &&
          !m_editor->cursorOnModelCoord()) {
        // This is a layout placeholder space, not really present in
        // the document, so don't draw it.
      }
      else {
        this->drawOneChar(paint, cursorFont, QPoint(x, baseline),
                          at(visibleText, visibleCursorColIndex),
                          false /*withinTrailingWhitespace*/);
      }
    }

    if (underlineCursor) {
      paint.setPen(cursorFont->getFgColor());
      drawUnderline(paint, x, ColumnCount(1));
    }
  }
}


void EditorWidget::drawSoftMarginIndicator(QPainter &paint)
{
  if (m_visibleSoftMargin) {
    QPainterSaveRestore qpsr(paint);

    paint.setPen(m_softMarginColor);

    ColumnIndex firstCol = firstVisibleCol();
    int x = m_leftMargin + m_fontWidth * (m_softMarginColumn - firstCol);
    paint.drawLine(x, 0, x, m_fontHeight-1);
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
        m_editor->m_namedDoc->highlightTrailingWhitespace()) {
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
    m_fontSet.minihex()->setSameFgBgColors(*font);

    drawHexQuad(*( m_fontSet.minihex() ), paint, bounds, codePoint);
  }
}


TextCategoryAndStyle EditorWidget::getTextCategoryAndStyle(
  TextCategoryAOA catAOA) const
{
  return TextCategoryAndStyle(
    m_fontSet,
    catAOA,
    getDocument()->m_modifiedOnDisk /*useDarker*/);
}


void EditorWidget::drawOffscreenMatchIndicators(QPainter &paint)
{
  // Use the same appearance as search hits, as that will help convey
  // what the numbers mean.
  TextCategoryAndStyle tcas(getTextCategoryAndStyle(
    TextCategoryAOA(TC_NORMAL, TOA_SEARCH_HIT)));
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
  LineCategoryAOAs &categories, LineIndex line)
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
        ColumnCount columns(lrange.m_end.m_column - lrange.m_start.m_column);

        // Double-check that the match is not zero columns.
        // Currently this cannot happen (if 'm_lengthBytes' is not
        // zero), but it will become possible if I lay out
        // zero-width characters properly.
        if (columns) {
          categories.overlay(
            lrange.m_start.m_column.get(),       // start
            columns.get(),                       // length
            TOA_SEARCH_HIT);                     // overlay
        }
      }
      else {
        // LineCategoryAOAs::overlay() interprets a zero length as
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

  TRACE2("keyPressEvent: " << keysString(*k));

  if (!this->hasFocus()) {
    // See doc/qt-focus-issues.txt.  This is a weird state, but I go
    // ahead anyway since my design is intended to be robust against
    // Qt screwing up its focus data and notifications.
    TRACE2("got a keystroke but I do not have focus!");

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
        COMMAND_MU(EC_MoveFirstVisibleConfineCursor,
          LineDifference(-1), ColumnDifference(0));
        break;

      case Qt::Key_Z:
        COMMAND_MU(EC_MoveFirstVisibleConfineCursor,
          LineDifference(+1), ColumnDifference(0));
        break;

      case Qt::Key_Up:
        commandMoveFirstVisibleAndCursor(
          LineDifference(-1), ColumnDifference(0));
        break;

      case Qt::Key_Down:
        commandMoveFirstVisibleAndCursor(
          LineDifference(+1), ColumnDifference(0));
        break;

      case Qt::Key_Left:
        commandMoveFirstVisibleAndCursor(
          LineDifference(0), ColumnDifference(-1));
        break;

      case Qt::Key_Right:
        commandMoveFirstVisibleAndCursor(
          LineDifference(0), ColumnDifference(+1));
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
        EDIT_COMMAND_MU(EC_DeleteKeyFunction);
        break;

      case Qt::Key_H:
        EDIT_COMMAND_MU(EC_BackspaceFunction);
        break;

      // I've decided I don't like this binding because I never use it
      // on purpose, and when I fat-finger it (while trying to press
      // Ctrl+K), the sudden jump in the location of text onscreen is
      // disorienting.
      #if 0
      case Qt::Key_L:
        COMMAND_MU(EC_CenterVisibleOnCursorLine);
        break;
      #endif

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
        commandMoveFirstVisibleAndCursor(
          LineDifference(-CTRL_SHIFT_DISTANCE), ColumnDifference(0));
        break;

      case Qt::Key_Down:
        commandMoveFirstVisibleAndCursor(
          LineDifference(+CTRL_SHIFT_DISTANCE), ColumnDifference(0));
        break;

      case Qt::Key_Left:
        commandMoveFirstVisibleAndCursor(
          LineDifference(0), ColumnDifference(-CTRL_SHIFT_DISTANCE));
        break;

      case Qt::Key_Right:
        commandMoveFirstVisibleAndCursor(
          LineDifference(0), ColumnDifference(+CTRL_SHIFT_DISTANCE));
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
          commandEditPaste(false /*cursorToStart*/);
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
        if (shift) {
          // I'm deliberately leaving Shift+Backspace unbound in
          // case I want to use it for something else later.
        }
        else {
          EDIT_COMMAND_MU(EC_BackspaceFunction);
        }
        break;
      }

      case Qt::Key_Delete: {
        if (shift) {
          this->commandEditCut();
        }
        else {
          EDIT_COMMAND_MU(EC_DeleteKeyFunction);
        }
        break;
      }

      case Qt::Key_Enter:
      case Qt::Key_Return: {
        if (shift) {
          // Shift+Enter is deliberately left unbound.
        }
        else {
          EDIT_COMMAND_MU(EC_InsertNewlineAutoIndent);
        }
        break;
      }

      case Qt::Key_Tab: {
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
          EDIT_COMMAND_MU(EC_InsertString, std::string("\t"));
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
          // Insert this character at the cursor.
          EDIT_COMMAND_MU(EC_InsertString, toString(text));
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

  TRACE3("keyRelease: " << keysString(*k));

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
    m_editor->setFirstVisibleLine(LineIndex(line));
    redraw();
  }
}

void EditorWidget::scrollToCol(int col)
{
  INITIATING_DOCUMENT_CHANGE();
  if (!m_ignoreScrollSignals) {
    xassert(col >= 0);
    m_editor->setFirstVisibleCol(ColumnIndex(col));
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

  LineIndex newLine( y/lineHeight() + this->firstVisibleLine().get() );
  ColumnIndex newCol( x/m_fontWidth + this->firstVisibleCol().get() );

  cursorTo(TextLCoord(newLine, newCol));

  // it's possible the cursor has been placed outside the "visible"
  // lines/cols (i.e. at the edge), but even if so, don't scroll,
  // because it messes up coherence with where the user clicked
}


std::string EditorWidget::cursorPositionUIString() const
{
  // I want the user to interact with line/col with a 1:1 origin, even
  // though the TextDocument interface uses 0:0.
  return stringb(
    cursorLine().toLineNumber() << ':' <<
    cursorCol().toColumnNumber());
}


void EditorWidget::mousePressEvent(QMouseEvent *m) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE2_GDVN_EXPRS("mousePressEvent", *m);

  INITIATING_DOCUMENT_CHANGE();

  m_editor->turnSelection(!!(m->modifiers() & Qt::ShiftModifier));
  setCursorToClickLoc(m);

  redraw();

  GENERIC_CATCH_END
}


void EditorWidget::mouseMoveEvent(QMouseEvent *m) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE3_GDVN_EXPRS("mouseMoveEvent", *m);

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

  TRACE2_GDVN_EXPRS("mouseReleaseEvent", *m);

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
  EDIT_COMMAND_MU(EC_Undo);
}


void EditorWidget::editRedo()
{
  EDIT_COMMAND_MU(EC_Redo);
}


static void setClipboard(string newText)
{
  QClipboard *cb = QApplication::clipboard();

  TRACE1("setClipboard: newText=" << doubleQuote(newText) <<
         " supportsSelection=" << cb->supportsSelection());

  cb->setText(toQString(newText), QClipboard::Clipboard);
  if (cb->supportsSelection()) {
    // Also set the X selection so I can paste it into an xterm.
    cb->setText(toQString(newText), QClipboard::Selection);
  }
}


void EditorWidget::commandEditCut()
{
  EDIT_COMMAND_MU(EC_Cut);
}


void EditorWidget::commandEditCopy()
{
  // This is not "EDIT_" because copying to the clipboard does not
  // change the document.
  COMMAND_MU(EC_Copy);
}


void EditorWidget::commandEditPaste(bool cursorToStart)
{
  EDIT_COMMAND_MU(EC_Paste, cursorToStart);
}


void EditorWidget::commandEditDelete()
{
  EDIT_COMMAND_MU(EC_DeleteMenuFunction);
}


void EditorWidget::commandEditKillLine()
{
  EDIT_COMMAND_MU(EC_KillLine);
}


void EditorWidget::commandEditSelectEntireFile()
{
  COMMAND_MU(EC_SelectEntireFile);
}


QRect EditorWidget::getCursorRect() const
{
  return QRect(
    (cursorCol() - this->firstVisibleCol()) * m_fontWidth,
    (cursorLine() - this->firstVisibleLine()).get() * m_fontHeight,
    m_fontWidth,
    m_fontHeight);
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
  QPoint target = getCursorRect().bottomLeft() + QPoint(0,1);

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
  return m_editor->m_namedDoc->highlightTrailingWhitespace();
}


FailReasonOpt EditorWidget::toggleHighlightTrailingWhitespace()
{
  NamedTextDocument *ntd = getDocument();

  FailReasonOpt reason =
    ntd->reasonCannotHighlightTrailingWhitespace();

  if (!reason) {
    ntd->setHighlightTrailingWhitespace(
      !ntd->highlightTrailingWhitespace());
  }

  return reason;
}


bool EditorWidget::getLSPUpdateContinuously() const
{
  return m_editor->m_namedDoc->m_lspUpdateContinuously;
}

bool EditorWidget::toggleLSPUpdateContinuously()
{
  return (( m_editor->m_namedDoc->m_lspUpdateContinuously =
              !m_editor->m_namedDoc->m_lspUpdateContinuously ));
}


void EditorWidget::commandCursorLeft(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell,
    LineDifference(0), ColumnDifference(-1), shift);
}

void EditorWidget::commandCursorRight(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell,
    LineDifference(0), ColumnDifference(+1), shift);
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
  COMMAND_MU(EC_MoveCursorByCell,
    LineDifference(-1), ColumnDifference(0), shift);
}

void EditorWidget::commandCursorDown(bool shift)
{
  COMMAND_MU(EC_MoveCursorByCell,
    LineDifference(+1), ColumnDifference(0), shift);
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
  m_editor->setFirstVisible(TextLCoord(LineIndex(0), ColumnIndex(0)));

  // Jump to the end of the document.  Even for a new process document,
  // there are a few lines of status information at the top.
  m_editor->moveCursorToBottom();
  m_editor->clearMark();

  // Bring the cursor line into view.
  m_editor->scrollToCursor();

  TRACE1("initCursorForProcessOutput: firstVisible: " <<
         m_editor->firstVisible());
}


std::string EditorWidget::markPositionUIString() const
{
  if (selectEnabled()) {
    TextLCoord m = mark();
    return stringb(
      m.m_line.toLineNumber() << ':' <<
      m.m_column.toColumnNumber());
  }
  else {
    return "none";
  }
}


void EditorWidget::setTextSearchParameters()
{
  m_textSearch->setSearchStringAndFlags(m_hitText, m_hitTextFlags);
}


void EditorWidget::setSearchStringParams(string const &searchString,
  TextSearch::SearchStringFlags flags, bool scrollToHit)
{
  TRACE2("setSearchStringParams: str=\"" << searchString <<
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
    TRACE2("scrollToNextSearchHit: " << (reverse? "prev" : "next") <<
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
    TRACE2("scrollToNextSearchHit: " << (reverse? "prev" : "next") <<
           " did not find anything");
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

  TRACE2("replaceSearchHit: " << DEBUG_VALUES3(existing, replaceSpec, replacement));

  EDIT_COMMAND_MU(EC_InsertString, replacement);

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


void EditorWidget::commandBlockIndent(ColumnDifference amt)
{
  EDIT_COMMAND_MU(EC_BlockIndent, amt);
}


// TODO: Make the indentation amount configurable.
void EditorWidget::commandEditRigidIndent()
{
  this->commandBlockIndent(ColumnDifference(+2));
}

void EditorWidget::commandEditRigidUnindent()
{
  this->commandBlockIndent(ColumnDifference(-2));
}


void EditorWidget::editJustifyParagraph()
{
  EDIT_COMMAND_MU(EC_JustifyNearCursor, m_softMarginColumn);
}


void EditorWidget::editInsertDateTime()
{
  INITIATING_DOCUMENT_CHANGE();
  TDE_HistoryGrouper grouper(*m_editor);
  m_editor->insertDateTime();
  this->redrawAfterContentChange();
}


void EditorWidget::editSelectionAsCString()
{
  std::string selection = getSelectedText();
  std::string escapedSelection = encodeWithEscapes(selection);

  bool ok;
  QString newEscapedText = QInputDialog::getText(this,
    "Edit Text",
    "Text using C string escapes:",
    QLineEdit::Normal,
    toQString(escapedSelection),
    &ok);

  if (ok) {
    try {
      std::string newText =
        decodeCStringEscapesToString(toString(newEscapedText));

      insertTextString(newText);
    }
    catch (XBase &x) {
      complain(x.getMessage());
      return;
    }
  }
}


void EditorWidget::insertText(char const *text, ByteCount length)
{
  insertTextString(stringBC(text, length));
}


void EditorWidget::insertTextString(std::string const &text)
{
  EDIT_COMMAND_MU(EC_InsertString, text);
}


// ----------------- nonfocus situation ------------------
void EditorWidget::focusInEvent(QFocusEvent *e) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE2("focusInEvent: this=" << (void*)this <<
         ", doc=" << this->getDocument()->documentName());
  QWidget::focusInEvent(e);

  // Refreshing when we gain focus interacts badly with the window that
  // pops up when a VFS operation is delayed.  Let's turn this off for
  // now.
  //this->requestFileStatus();

  editorGlobal()->addRecentEditorWidget(this);

  GENERIC_CATCH_END
}


void EditorWidget::focusOutEvent(QFocusEvent *e) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE2("focusOutEvent: this=" << (void*)this <<
         ", doc=" << this->getDocument()->documentName());
  QWidget::focusOutEvent(e);

  GENERIC_CATCH_END
}


// -------------------------------- LSP --------------------------------
NNRCSerf<LSPClientManager> EditorWidget::lspClientManager() const
{
  return editorGlobal()->lspClientManager();
}


RCSerfOpt<LSPClient const> EditorWidget::lspClientOptC() const
{
  return lspClientManager()->getClientOptC(getDocument());
}


RCSerfOpt<LSPClient const> EditorWidget::lspRunningClientOptC(
  bool wantErrors)
{
  // Make sure the document can have LSP services.
  if (FailReasonOpt reason =
        getDocument()->isIncompatibleWithLSP()) {
    if (wantErrors) {
      complain(*reason);
    }
    return {};
  }

  // Get the relevant client connection object.
  RCSerfOpt<LSPClient const> lspClient = lspClientOptC();
  if (!lspClient) {
    if (wantErrors) {
      complain(stringb(
        "No LSP connection is active for the scope of this document, "
        "which is " <<
        LSPClientScope::forNTD(getDocument()).description() << ".  "
        "Use the \"LSP | Start LSP server\" menu item."));
    }
    return {};
  }

  // If we just started the server, wait for it to finish initializing.
  // This is important during automated testing.
  if (!lspWaitUntilNotInitializing(lspClient)) {
    // User canceled the wait, no need for any error report.
    return {};
  }

  if (!lspClient->isRunningNormally()) {
    if (wantErrors) {
      complain(stringb("LSP server not ready: " <<
        lspClient->describeProtocolState()));
    }
    return {};
  }

  xassertPostcondition(lspClient->isRunningNormally());
  return lspClient;
}


std::optional<LSP_VersionNumber> EditorWidget::lspGetDocVersionNumber(
  bool wantErrors) const
{
  try {
    return LSP_VersionNumber::fromTDVN(getDocument()->getVersionNumber());
  }
  catch (XNumericConversion &x) {
    if (wantErrors) {
      complain(stringb(
        "The version number cannot be represented as an LSP int: " <<
        x.what()));
    }
    return {};
  }
}


bool EditorWidget::lspSynchronouslyWaitUntil(
  std::function<bool()> condition,
  std::string const &activityDialogMessage)
{
  // Record that we are waiting for an external process.  This prevents
  // the GUI test infrastructure from continuing with the next command
  // until this call completes.
  IncDecWaitingCounter idwc;

  return synchronouslyWaitUntil(this, condition, 500 /*ms*/,
           "Waiting for LSP server", activityDialogMessage);
}


bool EditorWidget::lspWaitUntilNotInitializing(
  LSPClient const *lspClient)
{
  if (lspClient->isInitializing()) {
    // Synchronously wait until LSP changes state.
    TRACE1("waiting for LSP to initialize");
    auto lambda = [lspClient]() -> bool {
      return (!lspClient->isInitializing());
    };
    std::string message = stringb(
      "Waiting for LSP server to start...");
    return lspSynchronouslyWaitUntil(lambda, message);
  }

  return true;
}


void EditorWidget::lspDoFileOperation(LSPFileOperation operation)
{
  // True if we want a popup for errors.
  bool const wantErrors = (operation != LSPFO_UPDATE_IF_OPEN);

  if (!lspRunningClientOptC(wantErrors)) {
    return;        // Error already reported if appropriate.
  }
  NNRCSerf<LSPClientManager> lcm = lspClientManager();

  // Is this file already open?
  NamedTextDocument * const ntd = getDocument();
  bool const alreadyOpen = lcm->fileIsOpen(ntd);

  if (operation == LSPFO_CLOSE) {
    if (!alreadyOpen) {
      if (wantErrors) {
        inform(stringb("Document " << ntd->documentName() <<
                       " is not open."));
      }
    }
    else {
      lcm->closeFile(ntd);
    }
    return;
  }

  if (operation == LSPFO_UPDATE_IF_OPEN && !alreadyOpen) {
    return;
  }

  try {
    if (!alreadyOpen) {
      if (FailReasonOpt languageIdOpt =
            lspLanguageIdForDTOpt(ntd->documentType())) {
        lcm->openFile(ntd, *languageIdOpt);
      }
      else {
        complain(stringb(
          "This editor application does not know how to interact "
          "with an LSP server for " <<
          languageName(ntd->documentType()) << " documents."));
      }
    }
    else /*update*/ {
      lcm->updateFile(ntd);
    }
  }
  catch (XBase &x) {
    if (wantErrors) {
      complain(x.getMessage());
    }
  }
}


void EditorWidget::lspUpdateFileIfContinuous()
{
  NamedTextDocument *ntd = getDocument();
  if (ntd->m_lspUpdateContinuously &&
      lspClientManager()->isRunningNormally(ntd) &&
      lspClientManager()->fileIsOpen(ntd)) {
    try {
      lspClientManager()->updateFile(ntd);
    }
    catch (XBase &x) {
      complain(stringb("LSP update: " << x));
      ntd->m_lspUpdateContinuously = false;
    }
  }
}


void EditorWidget::showDiagnosticDetailsDialog(
  QVector<DiagnosticElement> &&elts,
  std::string const &windowTitle)
{
  DiagnosticDetailsDialog *dlg =
    editorGlobal()->getDiagnosticDetailsDialog();

  dlg->setDiagnostics(std::move(elts));
  dlg->setWindowTitle(toQString(windowTitle));

  // Disconnect any previous connections for the "jump" signal.
  // This way the dialog object can be reused by any editor widget.
  QObject::disconnect(
    dlg, &DiagnosticDetailsDialog::signal_jumpToLocation,
    nullptr, nullptr);

  // Connect the signal to jump to location.
  //
  // Note: Qt connections are automatically removed if either object
  // is destroyed, so it is not a problem if this widget starts the
  // dialog and is then destroyed while the dialog is still open.
  QObject::connect(
    dlg, &DiagnosticDetailsDialog::signal_jumpToLocation,
    this, &EditorWidget::on_jumpToDiagnosticLocation);

  showRaiseAndActivateWindow(dlg);
}


auto EditorWidget::lspGetDiagnosticAtCursor() -> DiagnosticOrError
{
  if (TextDocumentDiagnostics const *tdd =
        getDocument()->getDiagnostics()) {
    TextMCoord cursorMC = m_editor->cursorAsModelCoord();
    if (std::optional<TDD_DocEntry> diag =
          tdd->getDiagnosticAt_orAtCollapsed(cursorMC)) {
      return *diag;
    }
    else {
      return "No diagnostics at cursor.";
    }
  }
  else {
    return "There are no diagnostics for this file.";
  }
}


FailReasonOpt EditorWidget::lspShowDiagnosticAtCursor(
  EditorNavigationOptions opts)
{
  SMFileUtil sfu;

  DiagnosticOrError diagOrError = lspGetDiagnosticAtCursor();
  if (diagOrError.isLeft()) {
    TDD_DocEntry const &diagDocEntry = diagOrError.left();
    TDD_Diagnostic const *diag = diagDocEntry.m_diagnostic;

    // Copy `diag` into a vector of elements for the dialog.
    QVector<DiagnosticElement> elts;

    // Primary location and message.
    DocumentName docName = getDocument()->documentName();
    elts.push_back(DiagnosticElement{
      docName.harn(),
      diagDocEntry.m_range.m_start,
      diag->m_message
    });

    // Related messages.
    for (TDD_Related const &rel : diag->m_related) {
      elts.push_back(DiagnosticElement{
        HostAndResourceName::localFile(rel.m_file),
        rel.m_loc,
        rel.m_message
      });
    }

    editorGlobal()->selectEditorWidget(this, opts)
      ->showDiagnosticDetailsDialog(std::move(elts),
          "Diagnostic Details");
    return {};
  }
  else {
    return diagOrError.right();
  }
}


FailReasonOpt EditorWidget::lspFixDiagnosticAtCursor()
{
  DiagnosticOrError diagOrError = lspGetDiagnosticAtCursor();
  if (diagOrError.isLeft()) {
    TDD_DocEntry const &diagDocEntry = diagOrError.left();
    TDD_Diagnostic const *diag = diagDocEntry.m_diagnostic;

    if (diag->m_fixes.empty()) {
      return "There are no proposed fixes.";
    }

    if (!editSafetyCheck()) {
      // The user canceled the edit.
      return {};
    }

    // Get titles of available fixes.
    std::vector<std::string> titles;
    for (TDD_ProposedFix const &pfix : diag->m_fixes) {
      std::string title = pfix.m_title;

      if (pfix.numFiles() != 1) {
        title += stringb(" (" << pfix.numFiles() << " files)");
      }

      if (pfix.numEdits() != 1) {
        title += stringb(" (" << pfix.numEdits() << " edits)");
      }

      titles.push_back(title);
    }

    // Let the user pick one.
    ListChoiceDialog dlg("Choose Fix", this);
    dlg.setChoices(titles);
    if (dlg.exec()) {
      // Apply the chosen fix.
      TDD_ProposedFix const &pfix = diag->m_fixes.at(dlg.chosenItem());

      if (pfix.numFiles() != 1) {
        return stringb("Chosen fix involves " << pfix.numFiles() <<
                       "; currently I can only do single-file fixes.");
      }

      auto [fname, edits] =
        (*( pfix.m_changesForFile.begin() ));

      DocumentName docName = DocumentName::fromLocalFilename(fname);
      if (getDocument()->documentName() != docName) {
        return stringb(
          "The chosen fix edits " << docName <<
          " but the current file is " << getDocument()->documentName() <<
          " and I can't currently handle editing a different file.");
      }

      // Combine all edits into one undo action.
      TDE_HistoryGrouper hbgrouper(*m_editor);

      for (TDD_TextEdit const &edit : edits) {
        RangeTextReplacement rtr(edit.m_range, edit.m_newText);

        EDIT_COMMAND_MU(EC_RangeTextReplace, rtr);
      }
    }

    return {};
  }
  else {
    return diagOrError.right();
  }
}


void EditorWidget::goToLocalFileAndLineOpt(
  std::string const &fname,
  std::optional<LineIndex> lineIndexOpt,
  std::optional<ByteIndex> byteIndexOpt)
{
  HostFile_OptLineByte hostFileAndLine(
    HostAndResourceName::localFile(fname),
    lineIndexOpt,
    byteIndexOpt);

  Q_EMIT signal_openOrSwitchToFileAtLineOpt(
    hostFileAndLine, false /*promptIfNotFound*/);
}


void EditorWidget::on_jumpToDiagnosticLocation(
  DiagnosticElement const &element) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_jumpToDiagnosticLocation:"
    " harn=" << element.m_harn <<
    " loc=" << element.m_loc);

  goToLocalFileAndLineOpt(
    element.m_harn.resourceName(),
    element.m_loc.m_line,
    element.m_loc.m_byteIndex);

  GENERIC_CATCH_END
}


void EditorWidget::lspGoToAdjacentDiagnostic(bool next)
{
  if (TextDocumentDiagnostics const *tdd =
        getDocument()->getDiagnostics()) {
    if (std::optional<TextMCoord> nextLoc =
          tdd->getAdjacentDiagnosticLocation(next,
            m_editor->cursorAsModelCoord())) {
      cursorTo(m_editor->toLCoord(*nextLoc));
      scrollToCursor(3 /*edgeGap*/);
      redraw();
    }
  }
}


void EditorWidget::lspGoToRelatedLocation(
  LSPSymbolRequestKind lsrk,
  EditorNavigationOptions options)
{
  RCSerfOpt<LSPClient const> client =
    lspRunningClientOptC(true /*wantErrors*/);
  if (!client) {
    return;        // Error already reported.
  }
  NNRCSerf<LSPClientManager> lcm = lspClientManager();

  NamedTextDocument *ntd = getDocument();
  if (!lcm->fileIsOpen(ntd)) {
    // Go ahead and open the file automatically.  This will entail more
    // delay than usual, but everything should work.
    lspDoFileOperation(LSPFO_OPEN_OR_UPDATE);

    if (!lcm->fileIsOpen(ntd)) {
      // Still not open, must have gotten an error, bail.
      return;
    }
  }

  TextMCoord coord = m_editor->cursorAsModelCoord();
  TRACE1("sending request for " << toString(lsrk) <<
         " of symbol in " << ntd->documentName() <<
         " at " << coord);
  int id = lcm->requestRelatedLocation(ntd, lsrk, coord);

  // Synchronously wait for the reply (or for the server to
  // malfunction).
  TRACE1("waiting for symbol information reply, id=" << id);
  auto lambda = [lcm, ntd, id]() -> bool {
    return (!lcm->isRunningNormally(ntd)) ||
           lcm->hasReplyForID(ntd, id);
  };
  std::string message = stringb(
    "Waiting for reply for " << toMessageString(lsrk) <<
    " request...");
  if (lspSynchronouslyWaitUntil(lambda, message)) {
    if (lcm->isRunningNormally(ntd)) {
      JSON_RPC_Reply reply = lcm->takeReplyForID(ntd, id);
      TRACE1("received reply: " << reply);

      if (reply.isError()) {
        complain(stringb("LSP error: " << reply.error().m_message));
      }
      else {
        EditorWidget *widgetToShow =
          editorGlobal()->selectEditorWidget(this, options);

        widgetToShow->lspHandleLocationReply(
          reply.result(), lsrk, client->uriPathSemantics());
      }
    }
    else {
      complain(lcm->explainAbnormality(ntd));
    }
  }
  else {
    TRACE1("canceled wait for " << toString(lsrk) << " reply");
    lcm->cancelRequestWithID(ntd, id);
  }
}


void EditorWidget::informNoSymbolInfo(
  LSPSymbolRequestKind lsrk)
{
  inform(stringb(
    "No " << toMessageString(lsrk) << " found for symbol at cursor."));
}


void EditorWidget::lspHandleLocationReply(
  GDValue const &gdvReply,
  LSPSymbolRequestKind lsrk,
  URIPathSemantics uriPathSemantics)
{
  if (gdvReply.isNull()) {
    informNoSymbolInfo(lsrk);
    return;
  }

  if (lsrk == LSPSymbolRequestKind::K_HOVER_INFO) {
    lspHandleHoverInfoReply(gdvReply);
    return;
  }

  if (lsrk == LSPSymbolRequestKind::K_COMPLETION) {
    lspHandleCompletionReply(gdvReply);
    return;
  }

  try {
    LSP_LocationSequence lseq{GDValueParser(gdvReply)};
    if (lseq.m_locations.empty()) {
      // Note that an empty sequence is different from `null`, which is
      // handled above.
      informNoSymbolInfo(lsrk);
    }
    else if (lseq.m_locations.size() == 1) {
      LSP_Location const &loc = lseq.m_locations.front();

      // TODO: Be able to select the entire range, rather than only
      // going to the start line/col.
      goToLocalFileAndLineOpt(
        loc.getFname(uriPathSemantics),
        loc.m_range.m_start.m_line,
        loc.m_range.m_start.m_character);
    }
    else {
      // Populate a vector of locations to query.
      std::vector<HostFileLine> locations;
      for (LSP_Location const &loc : lseq.m_locations) {
        locations.push_back(HostFileLine(
          HostAndResourceName::localFile(loc.getFname(uriPathSemantics)),
          loc.m_range.m_start.m_line));
      }

      // Query them all.  This does a synchronous wait.
      SynchronousWaiter waiter(this);
      if (std::optional<std::vector<std::string>> codeLines =
            lspClientManager()->getCodeLines(
              getDocument(), waiter, locations)) {
        xassert(codeLines->size() == locations.size());

        // Populate the information vector for the dialog.
        QVector<DiagnosticElement> elts;
        std::size_t i=0;
        for (LSP_Location const &loc : lseq.m_locations) {
          elts.push_back(DiagnosticElement{
            locations.at(i).getHarn(),
            toMCoord(loc.m_range.m_start),
            codeLines->at(i)
          });

          ++i;
        }

        // Show the results.
        showDiagnosticDetailsDialog(std::move(elts),
          stringb("Symbol query: " << toMessageString(lsrk)));
      }
      else {
        // User canceled the wait.
      }
    }
  }
  catch (XBase &x) {
    logAndWarnFailedLocationReply(gdvReply, lsrk, x);
  }
}


void EditorWidget::logAndWarnFailedLocationReply(
  GDValue const &gdvReply,
  LSPSymbolRequestKind lsrk,
  XBase &x)
{
  char const *lsrkMsgStr = toMessageString(lsrk);
  editorGlobal()->logAndWarn(this,
    stringb("Failed to parse " << lsrkMsgStr << " reply: " << x),
    stringb("Reply GDVN: " << gdvReply.asIndentedString())
  );
}


void EditorWidget::lspHandleHoverInfoReply(
  GDValue const &gdvReply)
{
  try {
    // Get the data to show from the reply, which for now I just treat
    // as a string.
    std::string message;
    {
      // Elsewhere I first parse the GDV into a more structured format
      // defined in `lsp-data`, but let's try just taking the GDV apart
      // directly here.
      GDValueParser top(gdvReply);
      GDValueParser contents = top.mapGetValueAtStr("contents");

      // The contents can be a string directly (which happens with
      // `pylsp` when I hover an invalid location) or a map with a
      // "value" attribute.
      if (contents.isString()) {
        message = contents.stringGet();
      }
      else {
        GDValueParser value = contents.mapGetValueAtStr("value");
        message = value.stringGet();
      }
    }

    if (message.empty()) {
      // This is how `pylsp` reports not having hover information.
      informNoSymbolInfo(LSPSymbolRequestKind::K_HOVER_INFO);
    }
    else {
      inform(message);
    }
  }
  catch (XBase &x) {
    logAndWarnFailedLocationReply(
      gdvReply, LSPSymbolRequestKind::K_HOVER_INFO, x);
  }
}


void EditorWidget::lspHandleCompletionReply(
  GDValue const &gdvReply)
{
  // Parse the incoming GDV.
  std::shared_ptr<LSP_CompletionList> clist;
  try {
    clist =
      std::make_shared<LSP_CompletionList>(GDValueParser(gdvReply));
  }
  catch (XBase &x) {
    logAndWarnFailedLocationReply(
      gdvReply, LSPSymbolRequestKind::K_COMPLETION, x);
    return;
  }

  // Calculate the widget-relative coordinate where we want the
  // completions dialog to appear.
  QPoint targetPt = getCursorRect().bottomRight() + QPoint(2,2);

  // Open the window that lets the user choose a completion.
  CompletionsDialog dlg(clist, targetPt, this);
  if (dlg.exec()) {
    if (auto selItemIndex = dlg.getSelectedItemIndex()) {
      // Edit to perform for this completion.
      LSP_TextEdit const &edit =
        listAtC(clist->m_items, *selItemIndex).m_textEdit;
      TRACE1("Applying completion edit: " << toGDValue(edit));

      // Ensure the coordinates are valid.
      TextMCoordRange modelRange = toMCoordRange(edit.m_range);
      m_editor->getDocument()->adjustMCoordRange(modelRange /*INOUT*/);

      // The editor interface works with layout coordinates.
      TextLCoordRange layoutRange = m_editor->toLCoordRange(modelRange);

      // Ensure the entire edit is one undo action.  (A single
      // `insertString` is already one action, but in the future I may
      // handle edits that also add #includes, etc.)
      TDE_HistoryGrouper grouper(*m_editor);

      // Select the affected text.
      m_editor->setSelectRange(layoutRange);

      // Replace it with the new text.
      EDIT_COMMAND_MU(EC_InsertString, edit.m_newText);
    }
    else {
      // The dialog is not supposed to allow accepting without anything
      // selected.
      DEV_WARNING("CompletionsDialog getSelectedItemIndex is nullopt");
    }
  }
}


void EditorWidget::lspSendSelectedText(bool asRequest)
{
  RCSerfOpt<LSPClient const> client =
    lspRunningClientOptC(true /*wantErrors*/);
  if (!client) {
    return;
  }
  NNRCSerf<LSPClientManager> lcm = lspClientManager();

  // Get the selected text.
  std::string selText = getSelectedText();
  if (selText.empty()) {
    complain("Nothing is selected.");
    return;
  }

  // Parse it as GDVN.
  GDValue gdvMessage;
  try {
    gdvMessage = fromGDVN(selText);
  }
  catch (XBase &x) {
    complain(x.getMessage());
    return;
  }

  // Substitute `CUR_FILE_URI` for its URL.
  NamedTextDocument const *ntd = getDocument();
  if (ntd->hasFilename()) {
    std::string curFileUri = client->makeFileURI(ntd->filename());
    gdvMessage = substitutionTransformGDValue(gdvMessage,
      std::map<GDValue, GDValue>{
        { "CUR_FILE_URI"_sym, curFileUri },
        { "CUR_FILE_VERSION"_sym, ntd->getVersionNumber() },
      });
  }

  if (!asRequest) {
    try {
      lspSendArbitraryNotification(gdvMessage);
    }
    catch (XBase &x) {
      complain(x.getMessage());
    }
    return;
  }

  // Get request method and params.
  std::string method;
  GDValue params;
  try {
    GDValueParser p(gdvMessage);
    method = p.mapGetValueAtStr("method").stringGet();
    params = p.mapGetValueAtStr("params").getValue();
  }
  catch (XBase &x) {
    complain(x.getMessage());
    return;
  }

  // Send these as a request.
  int requestID =
    lcm->sendArbitraryRequest(ntd, method, params);

  // Synchronously wait for the reply.
  IncDecWaitingCounter idwc;
  auto doneCondition = [lcm, ntd, requestID]() -> bool {
    return !lcm->isRunningNormally(ntd) ||
           lcm->hasReplyForID(ntd, requestID);
  };
  if (!synchronouslyWaitUntil(
         this,
         doneCondition,
         500 /*ms*/,
         "Waiting for LSP server",
         stringb("Waiting for reply to request ID " << requestID <<
                 ", method " << doubleQuote(method) << "."))) {
    // Canceled.
    return;
  }

  // Check if we stopped due to a protocol breakage.
  if (!lcm->isRunningNormally(ntd)) {
    complain(lcm->explainAbnormality(ntd));
    return;
  }

  // Take the reply.
  JSON_RPC_Reply reply = lcm->takeReplyForID(ntd, requestID);
  if (reply.isError()) {
    complain(stringb("LSP error: " << reply.error().m_message));
    return;
  }

  // Stringify it.
  std::string strReply = reply.result().asIndentedString();

  // `method` is not a file path of course, but often (always?) has a
  // slash, and when I try to set a window title to a string containing
  // a slash, Windows seemingly throws away everything before it.
  std::string abbrevMethod = withoutDirectoryPrefix(method.c_str());

  // The title includes the request ID, and hence almost guaranteed to
  // be unique, so it won't replace anything.
  std::string docTitle = stringb(
    "LSP reply " << requestID <<
    " to " << doubleQuote(abbrevMethod) << " method");

  // Put it into a document.
  NamedTextDocument *newDocument =
    editorGlobal()->getOrCreateGeneratedDocument(
      docTitle, strReply, DocumentType::DT_C_LIKE);

  // Show it.
  setDocumentFile(newDocument);
}


void EditorWidget::lspSendArbitraryNotification(
  GDValue const &gdvMessage)
{
  NNRCSerf<LSPClientManager> lcm = lspClientManager();
  NamedTextDocument const *ntd = getDocument();

  if (gdvMessage.isSequence()) {
    for (GDValue const &elt : gdvMessage.sequenceIterableC()) {
      // Since there could be many, check LSP health each time.
      if (!lcm->isRunningNormally(ntd)) {
        xmessage(lcm->explainAbnormality(ntd));
      }

      lspSendArbitraryNotification(elt);
    }
  }

  else {
    GDValueParser p(gdvMessage);
    std::string method = p.mapGetValueAtStr("method").stringGet();
    GDValue params = p.mapGetValueAtStr("params").getValue();

    lcm->sendArbitraryNotification(ntd, method, params);
  }
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

void EditorWidget::observeInsertLine(TextDocumentCore const &buf, LineIndex line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    TRACE2("IGNORING: observeInsertLine line=" << line);
    return;
  }
  TRACE2("observeInsertLine line=" << line);
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
  line.clampIncrease(LineDifference(-1));

  if (line <= m_editor->cursor().m_line) {
    m_editor->moveCursorBy(LineDifference(+1), ColumnDifference(0));
    if (keepCursorStationary) {
      m_editor->moveFirstVisibleBy(LineDifference(+1), ColumnDifference(0));
    }
    else {
      m_editor->scrollToCursor();
    }
  }

  if (m_editor->markActive() && line <= m_editor->mark().m_line) {
    m_editor->moveMarkBy(LineDifference(+1), ColumnDifference(0));
  }

  redrawAfterContentChange();
  GENERIC_CATCH_END
}

void EditorWidget::observeDeleteLine(TextDocumentCore const &buf, LineIndex line) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    TRACE2("IGNORING: observeDeleteLine line=" << line);
    return;
  }
  TRACE2("observeDeleteLine line=" << line);
  INITIATING_DOCUMENT_CHANGE();

  if (line < m_editor->cursor().m_line) {
    m_editor->moveCursorBy(LineDifference(-1), ColumnDifference(0));
    m_editor->moveFirstVisibleBy(LineDifference(-1), ColumnDifference(0));
  }

  if (m_editor->markActive() && line < m_editor->mark().m_line) {
    m_editor->moveMarkBy(LineDifference(-1), ColumnDifference(0));
  }

  redrawAfterContentChange();
  GENERIC_CATCH_END
}


// For inserted characters, I don't do anything special, so
// the cursor says in the same column of text.

void EditorWidget::observeInsertText(TextDocumentCore const &, TextMCoord, char const *, ByteCount) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redrawAfterContentChange();
  GENERIC_CATCH_END
}

void EditorWidget::observeDeleteText(TextDocumentCore const &, TextMCoord, ByteCount) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redrawAfterContentChange();
  GENERIC_CATCH_END
}

void EditorWidget::observeTotalChange(TextDocumentCore const &buf) NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  if (ignoringChangeNotifications()) {
    return;
  }
  redrawAfterContentChange();
  GENERIC_CATCH_END
}

void EditorWidget::observeMetadataChange(TextDocumentCore const &buf) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (ignoringChangeNotifications()) {
    return;
  }

  // This is a sort of bridge from the virtual method based observer
  // pattern to the Qt signals-and-slots pattern.  It allows the LSP
  // status widget to monitor for LSP diagnostics receipt without having
  // to directly watch the underlying document object.
  Q_EMIT signal_metadataChange();

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
      TRACE2("eventFilter: Rescuing Tab press");
      rescuedKeyPressEvent(keyEvent);
      return true;       // no further processing
    }
  }

  return false;

  GENERIC_CATCH_END_RET(false)
}


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
  TRACE2("command: " << toGDValue(*cmd).asString());

  NamedTextDocument *ntd = getDocument();
  TD_VersionNumber origVersion = ntd->getVersionNumber();

  if (FailReasonOpt msg = innerCommand(cmd.get())) {
    QMessageBox::information(this, "Error", toQString(*msg));
  }
  else {
    editorGlobal()->recordCommand(std::move(cmd));
  }

  // Update LSP if the document changed.
  if (origVersion != ntd->getVersionNumber()) {
    lspUpdateFileIfContinuous();
  }
}


FailReasonOpt EditorWidget::innerCommand(EditorCommand const *cmd)
{
  // As this is where we act on the command to make a change, suppress
  // notifications here that might be caused by the change.
  INITIATING_DOCUMENT_CHANGE();

  // Handle undo/redo first, before the catch-all undo grouper.
  ASTSWITCHC(EditorCommand, cmd) {
    ASTCASEC1(EC_Undo) {
      if (m_editor->canUndo()) {
        m_editor->undo();
        this->redrawAfterContentChange();
        return std::nullopt;
      }
      else {
        return "There are no actions to undo in the history.";
      }
    }

    ASTNEXTC1(EC_Redo) {
      if (m_editor->canRedo()) {
        m_editor->redo();
        this->redrawAfterContentChange();
        return std::nullopt;
      }
      else {
        return "There are no actions to redo in the history.";
      }
    }

    ASTENDCASECD
  }

  // Group everything arising from one command.
  TDE_HistoryGrouper hbgrouper(*m_editor);

  // The cases here should be in the same order as in
  // `editor-command.ast`.
  ASTSWITCHC(EditorCommand, cmd) {
    // ---------------------------- Cursor -----------------------------
    ASTCASEC(EC_MoveCursorByCell, ec) {
      m_editor->turnSelection(ec->m_select);
      m_editor->moveCursorBy(ec->m_deltaLine, ec->m_deltaColumn);
      scrollToCursor();
    }

    ASTNEXTC(EC_MoveCursorByPage, ec) {
      m_editor->turnSelection(ec->m_select);
      m_editor->moveFirstVisibleAndCursor(
        LineDifference(ec->m_sign * this->visLines()),
        ColumnDifference(0));
      redraw();
    }

    ASTNEXTC(EC_MoveCursorToLineExtremum, ec) {
      m_editor->turnSelection(ec->m_select);
      if (ec->m_start) {
        m_editor->setCursorColumn(ColumnIndex(0));
      }
      else {
        m_editor->setCursorColumn(
          m_editor->cursorLineLengthAsColumnIndex());
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

    ASTNEXTC(EC_CursorToEndOfNextLine, ec) {
      // TODO: Encapsulate this as an editor method.
      m_editor->turnSelection(ec->m_select);
      LineIndex line = m_editor->cursor().m_line;
      m_editor->setCursor(m_editor->lineEndLCoord(line.succ()));
      scrollToCursor();
    }

    // --------------------------- Selection ---------------------------
    ASTNEXTC1(EC_SelectEntireFile) {
      m_editor->selectEntireFile();
      this->redraw();
    }

    // --------------------------- Scrolling ---------------------------
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

    ASTNEXTC1(EC_CenterVisibleOnCursorLine) {
      m_editor->centerVisibleOnCursorLine();
      redraw();
    }

    // ------------------------ Text insertion -------------------------
    ASTNEXTC(EC_InsertString, ec) {
      m_editor->insertString(ec->m_text);
      this->redrawAfterContentChange();
    }

    // ------------------------- Text deletion -------------------------
    ASTNEXTC1(EC_BackspaceFunction) {
      m_editor->backspaceFunction();
      redrawAfterContentChange();
    }

    ASTNEXTC1(EC_DeleteKeyFunction) {
      m_editor->deleteKeyFunction();
      redrawAfterContentChange();
    }

    ASTNEXTC1(EC_DeleteMenuFunction) {
      if (selectEnabled()) {
        m_editor->deleteSelection();
        redrawAfterContentChange();
      }
    }

    // ----------------------- Text replacement ------------------------
    ASTNEXTC(EC_RangeTextReplace, ec) {
      m_editor->applyRangeTextReplacement(ec->m_rtr);
      redrawAfterContentChange();
    }

    // ----------------------- Adding whitespace -----------------------
    ASTNEXTC1(EC_InsertNewlineAutoIndent) {
      m_editor->insertNewlineAutoIndent();
      redrawAfterContentChange();
    }

    ASTNEXTC(EC_BlockIndent, ec) {
      if (m_editor->blockIndent(ec->m_amt)) {
        redrawAfterContentChange();
      }
    }

    ASTNEXTC(EC_JustifyNearCursor, ec) {
      if (selectEnabled()) {
        // TODO: This.
        return "Unimplemented: justification of selected region.";
      }
      else {
        m_editor->justifyNearCursor(ec->desiredWidth);
        redrawAfterContentChange();
      }
    }

    // --------------------------- Clipboard ---------------------------
    ASTNEXTC1(EC_Copy) {
      if (this->selectEnabled()) {
        setClipboard(m_editor->clipboardCopy());
        this->redraw();
      }
    }

    ASTNEXT1(EC_Cut) {
      if (this->selectEnabled()) {
        setClipboard(m_editor->clipboardCut());
        this->redrawAfterContentChange();
      }
    }

    ASTNEXTC1(EC_KillLine) {
      // TODO: Encapsulate as an editor method.
      if (!selectEnabled()) {
        m_editor->selectCursorLine();
      }
      setClipboard(m_editor->clipboardCut());
      this->redrawAfterContentChange();
    }

    ASTNEXTC(EC_Paste, ec) {
      QClipboard *cb = QApplication::clipboard();
      QString text;

      // Try reading the X selection first.  Generally this seems to reflect
      // the "more recent" deliberate clipboard interaction.
      if (cb->supportsSelection()) {
        text = cb->text(QClipboard::Selection);
        TRACE1("EC_Paste: Got selection: " << doubleQuote(text));
      }

      // Then the regular clipboard.
      if (text.isEmpty()) {
        text = cb->text(QClipboard::Clipboard);
        TRACE1("EC_Paste: Got clipboard: " << doubleQuote(text));
      }

      // Previously, I had a check here for empty `text`, and a warning
      // dialog.  But I want the processing of command objects to not
      // rely on being interactive, and the warning served little real
      // purpose, so I removed it.

      QByteArray utf8(text.toUtf8());
      m_editor->clipboardPaste(utf8.constData(), ByteCount(utf8.length()),
                               ec->m_cursorToStart);
      this->redrawAfterContentChange();
    }

    // Undo and redo are handled in the `switch` above this one.

    ASTENDCASECD
  }

  return std::nullopt;
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
  box.addButton(QMessageBox::No);

  // If I don't realize something is read-only, I often type quickly
  // into a document, including pressing Enter.  The default default is
  // "Yes", which means I'm suddenly editing the file when I didn't mean
  // to because I didn't notice the popup.  By setting it to "No", an
  // errant Enter press will not change the document.
  box.setDefaultButton(QMessageBox::No);

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


GDValue EditorWidget::eventReplayQuery(string const &state)
{
  if (state == "firstVisible") {
    return stringb(m_editor->firstVisible());
  }
  else if (state == "lastVisible") {
    return stringb(m_editor->lastVisible());
  }
  else if (state == "documentProcessState") {
    return toGDValue(m_editor->documentProcessStatus());
  }
  else if (state == "hasUnsavedChanges") {
    return m_editor->unsavedChanges();
  }
  else if (state == "resourceName") {
    return m_editor->m_namedDoc->resourceName();
  }
  else if (state == "documentFileName") {
    // Strip path info.
    return SMFileUtil().splitPathBase(m_editor->m_namedDoc->resourceName());
  }
  else if (state == "documentType") {
    return languageName(m_editor->m_namedDoc->documentType());
  }
  else if (state == "documentText") {
    return m_editor->getTextForLRangeString(m_editor->documentLRange());
  }
  else if (state == "selectedText") {
    return m_editor->getSelectedText();
  }
  else if (state == "cursorPosition") {
    return cursorPositionUIString();
  }
  else if (state == "markPosition") {
    return markPositionUIString();
  }
  else if (state == "highlightTrailingWhitespace") {
    return highlightTrailingWhitespace();
  }
  else if (state == "lspIsFakeServer") {
    return editorGlobal()->lspIsFakeServer();
  }
  else if (state == "lspIsRunningNormally") {
    return lspClientManager()->isRunningNormally(getDocument());
  }
  else if (state == "lspNumDiagnostics") {
    // Returns a number or "null".
    return toGDValue(getDocument()->getNumDiagnostics());
  }
  else if (state == "selfCheck") {
    // Just invoke self check, throwing if it fails.  The returned
    // string is not meaningful.
    //
    // Note: This should be unnecessary since the event replay
    // infrastructure does a global self-check after every event.
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
