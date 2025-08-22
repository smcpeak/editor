// lsp-status-widget.cc
// Code for `lsp-status-widget` module.

#include "lsp-status-widget.h"         // this module

#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "lsp-manager.h"               // LSPManager

#include "smqtutil/qstringb.h"         // qstringb

#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition


INIT_TRACE("lsp-status-widget");


LSPStatusWidget::~LSPStatusWidget()
{
  // The signal connections should already be disconnected, and by the
  // time we get here, the widget pointer should be null so we cannot
  // disconnect anymore.
}


LSPStatusWidget::LSPStatusWidget(
  EditorWidget *editorWidget, QWidget *parent)
  : QLabel(parent),
    IMEMBFP(editorWidget),
    m_fakeStatus(0)
{
  xassertPrecondition(editorWidget);

  setAlignment(Qt::AlignCenter);

  // These are disconnected in `resetEditorWidget`.
  QObject::connect(
    editorGlobal(), &EditorGlobal::signal_lspChangedProtocolState,
    this, &LSPStatusWidget::on_changedLSPStatus);
  QObject::connect(
    m_editorWidget, &EditorWidget::signal_metadataChange,
    this, &LSPStatusWidget::on_changedLSPStatus);
  QObject::connect(
    m_editorWidget, &EditorWidget::signal_contentChange,
    this, &LSPStatusWidget::on_changedLSPStatus);

  // Configure for initial state.
  on_changedLSPStatus();
}


void LSPStatusWidget::setBackgroundColor(QColor color)
{
  // TODO: This is inelegant.  Perhaps I can set a single style sheet
  // and then configure the class, like in HTML?
  setStyleSheet(qstringb(
    "QLabel { background-color: rgb(" <<
    color.red() << ", " <<
    color.green() << ", " <<
    color.blue() << "); }"));
}


EditorGlobal *LSPStatusWidget::editorGlobal() const
{
  return m_editorWidget->editorGlobal();
}


void LSPStatusWidget::resetEditorWidget()
{
  if (m_editorWidget) {
    QObject::disconnect(editorGlobal(), nullptr, this, nullptr);

    QObject::disconnect(m_editorWidget, nullptr, this, nullptr);
    m_editorWidget = nullptr;
  }
}


void LSPStatusWidget::on_changedLSPStatus() noexcept
{
  GENERIC_CATCH_BEGIN

  // Palette of colors that indicate various states.
  static QColor const inactiveColor     (192, 192, 192); // light gray
  static QColor const waitingColor      (255, 128, 255); // pink
  static QColor const zeroDiagsColor    (102, 255, 102); // light green
  static QColor const hasDiagsColor     (255, 223, 128); // light yellow with hint of orange
  static QColor const transitioningColor(128, 128, 255); // light blue/purple
  static QColor const protoErrorColor   (255, 100, 100); // soft red
  static QColor const internalErrorColor(255,   0,   0); // hard red

  // Background color to use.
  QColor bgColor;

  // Main bit of text to show.
  std::string text;

  // Whether to follow that with an asterisk, which means the
  // information is out of date due to the user modifying the file.
  bool addAsterisk = false;

  LSPProtocolState state = editorGlobal()->lspGetProtocolState();
  if (m_fakeStatus) {
    state = static_cast<LSPProtocolState>(m_fakeStatus-1);
  }
  TRACE2("on_changedLSPStatus: " << toString(state));

  switch (state) {
    case LSP_PS_MANAGER_INACTIVE:
      text = "_";
      bgColor = inactiveColor;
      break;

    case LSP_PS_INITIALIZING:
      text = "I";
      bgColor = transitioningColor;
      break;

    case LSP_PS_NORMAL: {
      // Text shows the count of diagnostics, if we have any.
      NamedTextDocument const *doc = m_editorWidget->getDocument();
      std::optional<int> numDiagnostics = doc->getNumDiagnostics();
      if (numDiagnostics.has_value()) {
        text = stringb(*numDiagnostics);
      }
      else {
        text = "-";
      }

      // The asterisk means the count is for an older version.
      if (doc->hasOutOfDateDiagnostics()) {
        addAsterisk = true;
      }

      // Color indicates protocol state and (under otherwise normal
      // circumstances) number of diagnostics.
      RCSerf<LSPDocumentInfo const> lspDocInfo =
        editorGlobal()->lspGetDocInfo(doc);
      if (!lspDocInfo) {
        TRACE2("  on_changedLSPStatus: inactive color");
        bgColor = inactiveColor;
      }
      else if (lspDocInfo->m_waitingForDiagnostics) {
        TRACE2("  on_changedLSPStatus: waiting color");
        bgColor = waitingColor;
      }
      else if (numDiagnostics.has_value()) {
        if (*numDiagnostics == 0) {
        TRACE2("  on_changedLSPStatus: zeroDiags color");
          bgColor = zeroDiagsColor;
        }
        else {
        TRACE2("  on_changedLSPStatus: hasDiags color");
          bgColor = hasDiagsColor;
        }
      }
      else {
        // No diagnostics, but also not inactive or waiting?
        TRACE2("  on_changedLSPStatus: internal error color");
        bgColor = internalErrorColor;
      }

      break;
    }

    case LSP_PS_SHUTDOWN1:
      text = "S1";
      bgColor = transitioningColor;
      break;

    case LSP_PS_SHUTDOWN2:
      text = "S2";
      bgColor = transitioningColor;
      break;

    case LSP_PS_PROTOCOL_ERROR:
      text = "E";
      bgColor = protoErrorColor;
      break;

    case LSP_PS_PROTOCOL_OBJECT_MISSING:
      text = "B1";
      bgColor = internalErrorColor;
      break;

    case LSP_PS_SERVER_NOT_RUNNING:
      text = "B2";
      bgColor = internalErrorColor;
      break;

    default:
      text = "?";
      bgColor = internalErrorColor;
      break;
  }

  std::string finalText = stringb(text << (addAsterisk? "*" : ""));
  TRACE2("  on_changedLSPStatus: text: " << finalText);

  setText(toQString(finalText));
  setBackgroundColor(bgColor);

  GENERIC_CATCH_END
}


// EOF
