// diagnostic-details-dialog.h
// `DiagnosticDetailsDialog`, showing details of a language diagnostic.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DIAGNOSTIC_DETAILS_DIALOG_H
#define EDITOR_DIAGNOSTIC_DETAILS_DIALOG_H

#include "diagnostic-element-fwd.h"              // DiagnosticElement [n]
#include "event-replay.h"                        // EventReplayQueryable

#include "smqtutil/sm-table-widget-fwd.h"        // SMTableWidget

#include "smbase/refct-serf.h"                   // SerfRefCount

#include <QDialog>
#include <QString>
#include <QVector>

#include <memory>                                // std::unique_ptr

class QLabel;
class QPlainTextEdit;
class QSplitter;


// This dialog allows the user to review the details of one compiler
// diagnostic and jump to relevant source locations.
//
// See `doc/diagnostic-details-spec.html` for details.
//
class DiagnosticDetailsDialog : public QDialog,
                                public SerfRefCount,
                                public EventReplayQueryable {
  Q_OBJECT

private:     // data
  // Sequence of elements being shown.  The first is the "main" one,
  // identifying some problem, while others are supporing evidence.
  QVector<DiagnosticElement> m_diagnostics;

  // Controls.
  QLabel *m_locationLabel;             // Selected element file/line.
  QPlainTextEdit *m_messageText;       // Selected element message.
  QSplitter *m_splitter;               // Resize the panels.
  SMTableWidget *m_table;              // Table of elements.

private:     // methods
  // Update the top panel to show the details of the selected table row.
  void updateTopPanel();

  // Populate the table from `m_diagnostics`.
  void repopulateTable();

private Q_SLOTS:
  // React to the table's selected row changing.
  void on_tableSelectionChanged() noexcept;

protected:   // methods
  virtual void keyPressEvent(QKeyEvent *event) override;
  virtual void showEvent(QShowEvent *event) override;

public:      // methods
  ~DiagnosticDetailsDialog();

  DiagnosticDetailsDialog(QWidget *parent = nullptr);

  // Replace the current set of diagnostics.
  void setDiagnostics(QVector<DiagnosticElement> &&diagnostics);

  // EventReplayQueryable methods.
  virtual string eventReplayQuery(string const &state) override;

Q_SIGNALS:
  // Emitted when the user indicates they want to see one element in
  // an editor.
  void signal_jumpToLocation(DiagnosticElement const &element);
};


#endif // EDITOR_DIAGNOSTIC_DETAILS_DIALOG_H
