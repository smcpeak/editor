// lsp-status-widget.cc
// Code for `lsp-status-widget` module.

#include "lsp-status-widget.h"         // this module

#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "lsp-manager.h"               // LSPManager

#include "smqtutil/qstringb.h"         // qstringb

#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // joinTerminate
#include "smbase/xassert.h"            // xassertPrecondition

#include <QMessageBox>
#include <QPainter>

#include <string>                      // std::string
#include <vector>                      // std::vector


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
    m_bgColor(),
    m_statusReport(),
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


void LSPStatusWidget::mousePressEvent(QMouseEvent *e)
{
  QMessageBox mbox(this);
  mbox.setObjectName("statusReportBox");
  mbox.setWindowTitle("LSP Status Report");
  mbox.setText(toQString(m_statusReport));
  mbox.exec();
}


void LSPStatusWidget::paintEvent(QPaintEvent *e)
{
  QPainter painter(this);

  painter.fillRect(rect(), m_bgColor);

  QLabel::paintEvent(e);
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

  // Background color to change to.
  QColor bgColor;

  // Main bit of text to show.
  std::string text;

  // Messages for the status report.
  std::vector<std::string> statusMessages;

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
      statusMessages.push_back("The LSP server is inactive.");
      break;

    case LSP_PS_INITIALIZING:
      text = "I";
      bgColor = transitioningColor;
      statusMessages.push_back("The LSP server is initializing.");
      break;

    case LSP_PS_NORMAL: {
      NamedTextDocument const *doc = m_editorWidget->getDocument();
      statusMessages.push_back(stringb(
        "Current document version is " << doc->getVersionNumber() << "."));

      RCSerf<LSPDocumentInfo const> lspDocInfo =
        editorGlobal()->lspGetDocInfo(doc);
      if (!lspDocInfo) {
        TRACE2("  on_changedLSPStatus: inactive color");
        bgColor = inactiveColor;
        text = "-";
        statusMessages.push_back(
          "This document is not open with the LSP server.");
        break;
      }

      // Text shows the count of diagnostics, if we have any.
      std::optional<int> numDiagnostics = doc->getNumDiagnostics();
      if (numDiagnostics.has_value()) {
        text = stringb(*numDiagnostics);

        statusMessages.push_back(stringb(
          "There are " << *numDiagnostics << " diagnostics."));

        if (doc->hasOutOfDateDiagnostics()) {
          addAsterisk = true;
          statusMessages.push_back(stringb(
            "The diagnostics are based on version " <<
            doc->getDiagnosticsOriginVersion().value() <<
            ", meaning they are out of date (indicated by the asterisk)."));
        }
        else {
          statusMessages.push_back(stringb(
            "The diagnostics are based on the current version (" <<
            doc->getDiagnosticsOriginVersion().value() << ")."));
        }
      }
      else {
        text = "-";
        statusMessages.push_back(
          "No diagnostic report has been received.");
      }

      if (lspDocInfo->m_waitingForDiagnostics) {
        TRACE2("  on_changedLSPStatus: waiting color");
        bgColor = waitingColor;
        statusMessages.push_back(
          numDiagnostics.has_value()?
            "We are waiting for the LSP server to provide updated diagnostics." :
            "We are waiting for the LSP server to provide the first diagnostics.");
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
        // No diagnostics, but also not inactive or waiting?  This
        // should not be possible.
        TRACE2("  on_changedLSPStatus: internal error color");
        bgColor = internalErrorColor;
        statusMessages.push_back(
          "Internal error: No diagnostics, not inactive, not waiting.");
      }

      break;
    }

    case LSP_PS_SHUTDOWN1:
      text = "S1";
      bgColor = transitioningColor;
      statusMessages.push_back(
        "The LSP connection is in phase 1 shutdown.");
      break;

    case LSP_PS_SHUTDOWN2:
      text = "S2";
      bgColor = transitioningColor;
      statusMessages.push_back(
        "The LSP connection is in phase 2 shutdown.");
      break;

    case LSP_PS_JSON_RPC_PROTOCOL_ERROR:
    case LSP_PS_MANAGER_PROTOCOL_ERROR:
      text = "E";
      bgColor = protoErrorColor;
      statusMessages.push_back(stringb(
        "There was an LSP protocol error: " <<
        editorGlobal()->lspExplainAbnormality()));
      break;

    case LSP_PS_PROTOCOL_OBJECT_MISSING:
      text = "B1";
      bgColor = internalErrorColor;
      statusMessages.push_back(
        "LSP internal error: Protocol object missing.");
      break;

    case LSP_PS_SERVER_NOT_RUNNING:
      text = "B2";
      bgColor = internalErrorColor;
      statusMessages.push_back(
        "LSP internal error: Server not running.");
      break;

    default:
      text = "?";
      bgColor = internalErrorColor;
      statusMessages.push_back(stringb(
        "LSP internal error: Unknown protocol state: " << (int)state));
      break;
  }

  std::string finalText = stringb(text << (addAsterisk? "*" : ""));
  TRACE2("  on_changedLSPStatus: text: " << finalText);

  setText(toQString(finalText));
  if (bgColor != m_bgColor) {
    m_bgColor = bgColor;
    update();
  }
  m_statusReport = join(statusMessages, "\n");

  GENERIC_CATCH_END
}


// EOF
