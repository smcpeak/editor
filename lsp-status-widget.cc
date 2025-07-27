// lsp-status-widget.cc
// Code for `lsp-status-widget` module.

#include "lsp-status-widget.h"         // this module

#include "editor-global.h"             // EditorGlobal
#include "editor-widget.h"             // EditorWidget
#include "lsp-manager.h"               // LSPManager

#include "smqtutil/qtutil.h"           // qstringb


LSPStatusWidget::~LSPStatusWidget()
{
  QObject::disconnect(lspManager(), nullptr, this, nullptr);
}


LSPStatusWidget::LSPStatusWidget(
  EditorWidget *editorWidget, QWidget *parent)
  : QLabel(parent),
    m_editorWidget(editorWidget),
    m_fakeStatus(0)
{
  setAlignment(Qt::AlignCenter);

  // Monitor for changes via signals.
  QObject::connect(
    lspManager(), &LSPManager::signal_changedProtocolState,
    this,        &LSPStatusWidget::on_changedProtocolState);

  // Configure for initial state.
  on_changedProtocolState();
}


void LSPStatusWidget::setBackgroundColor(int r, int g, int b)
{
  // TODO: This is inelegant.  Perhaps I can set a single style sheet
  // and then configure the class, like in HTML?
  setStyleSheet(qstringb(
    "QLabel { background-color: rgb(" <<
    r << ", " << g << ", " << b << "); }"));
}


LSPManager *LSPStatusWidget::lspManager() const
{
  return &( m_editorWidget->editorGlobal()->m_lspManager );
}


void LSPStatusWidget::on_changedProtocolState() noexcept
{
  GENERIC_CATCH_BEGIN

  LSPProtocolState state = lspManager()->getProtocolState();

  if (m_fakeStatus) {
    state = static_cast<LSPProtocolState>(m_fakeStatus-1);
  }

  switch (state) {
    case LSP_PS_MANAGER_INACTIVE:
      setText("_");
      setBackgroundColor(0xC0, 0xC0, 0xC0);   // light gray
      break;

    case LSP_PS_INITIALIZING:
      setText("I");
    lightBlue:
      setBackgroundColor(117, 196, 238);      // light blue
      break;

    case LSP_PS_NORMAL:
      setText("R");
      setBackgroundColor(102, 255, 102);      // light green
      break;

    case LSP_PS_SHUTDOWN1:
      setText("S1");
      goto lightBlue;

    case LSP_PS_SHUTDOWN2:
      setText("S2");
      goto lightBlue;

    case LSP_PS_PROTOCOL_ERROR:
      setText("E");
      setBackgroundColor(255, 100, 100);      // soft red
      break;

    case LSP_PS_PROTOCOL_OBJECT_MISSING:
      setText("B1");
      setBackgroundColor(0xFF, 0x00, 0x00);   // hard red
      break;

    case LSP_PS_SERVER_NOT_RUNNING:
      setText("B2");
      setBackgroundColor(0xFF, 0x00, 0x00);   // hard red
      break;

    default:
      setText("?");
      setBackgroundColor(0xFF, 0x00, 0x00);   // hard red
      break;
  }

  GENERIC_CATCH_END
}


// EOF
