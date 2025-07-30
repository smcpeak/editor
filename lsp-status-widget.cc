// lsp-status-widget.cc
// Code for `lsp-status-widget` module.

#include "lsp-status-widget.h"         // this module

#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "lsp-doc-state.h"             // LSPDocumentState
#include "lsp-manager.h"               // LSPManager

#include "smqtutil/qstringb.h"         // qstringb

#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/xassert.h"            // xassertPrecondition


LSPStatusWidget::~LSPStatusWidget()
{
  // The editor widget signal connection should already be disconnected.
}


LSPStatusWidget::LSPStatusWidget(
  EditorWidget *editorWidget, QWidget *parent)
  : QLabel(parent),
    IMEMBFP(editorWidget),
    m_fakeStatus(0)
{
  xassertPrecondition(editorWidget);

  setAlignment(Qt::AlignCenter);

  // Monitor for changes via signals.
  QObject::connect(
    lspManager(), &LSPManager::signal_changedProtocolState,
    this,        &LSPStatusWidget::on_changedLSPStatus);

  // These are disconnected in `resetEditorWidget`.
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


LSPManager *LSPStatusWidget::lspManager() const
{
  return &( m_editorWidget->editorGlobal()->m_lspManager );
}


void LSPStatusWidget::resetEditorWidget()
{
  if (m_editorWidget) {
    QObject::disconnect(m_editorWidget, nullptr, this, nullptr);
    m_editorWidget = nullptr;
  }
}


void LSPStatusWidget::on_changedLSPStatus() noexcept
{
  GENERIC_CATCH_BEGIN

  // Palette of colors that indicate various states.
  static QColor const inactiveColor     (192, 192, 192); // light gray
  static QColor const transitioningColor(255, 128, 255); // light purple
  static QColor const zeroDiagsColor    (102, 255, 102); // light green
  static QColor const hasDiagsColor     (255, 223, 128); // light yellow with hint of orange
  static QColor const waitingColor      (128, 255, 255); // cyan
  static QColor const staleColor        (255, 128,  64); // light orange
  static QColor const protoErrorColor   (255, 100, 100); // soft red
  static QColor const internalErrorColor(255,   0,   0); // hard red

  // Background color to use.
  QColor bgColor;

  // Main bit of text to show.
  std::string text;

  // Whether to follow that with an asterisk, which means the
  // information is out of date due to the user modifying the file.
  bool addAsterisk = false;

  LSPProtocolState state = lspManager()->getProtocolState();
  if (m_fakeStatus) {
    state = static_cast<LSPProtocolState>(m_fakeStatus-1);
  }

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
      std::optional<int> numDiagnostics =
        m_editorWidget->getDocument()->getNumDiagnostics();
      if (numDiagnostics.has_value()) {
        text = stringb(*numDiagnostics);
      }
      else {
        text = "-";
      }

      // Color and asterisk indicates the state.
      LSPDocumentState state =
        m_editorWidget->getDocument()->getLSPDocumentState();
      switch (state) {
        case LSPDS_NOT_OPEN:
          bgColor = inactiveColor;
          break;

        case LSPDS_LOCAL_CHANGES:
          addAsterisk = true;
          // fallthrough

        case LSPDS_UP_TO_DATE:
          xassert(numDiagnostics.has_value());
          bgColor =
            (numDiagnostics == 0)? zeroDiagsColor : hasDiagsColor;
          break;

        case LSPDS_WAITING:
          // Waiting for updated diagnostics.
          if (numDiagnostics.has_value()) {
            addAsterisk = true;
          }
          bgColor = waitingColor;
          break;

        case LSPDS_RECEIVED_STALE:
          bgColor = staleColor;
          break;

        default:
          bgColor = internalErrorColor;
          break;
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

  setText(qstringb(text << (addAsterisk? "*" : "")));
  setBackgroundColor(bgColor);

  GENERIC_CATCH_END
}


// EOF
