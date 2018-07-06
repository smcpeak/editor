// command-runner.h
// Declare the CommandRunner class.

#ifndef COMMAND_RUNNER_H
#define COMMAND_RUNNER_H

#include "xassert.h"                   // xassert

#include <QByteArray>
#include <QEventLoop>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>

// Run a command on some input, gather the output.
class CommandRunner : public QObject {
  Q_OBJECT

private:     // data
  // Event loop object used to implement the blocking interface.
  QEventLoop m_eventLoop;

  // Timer ID to implement a timeout on an inactive process.
  int m_timerId;

  // The underlying QProcess object that we use to start and
  // communicate with the child process.
  QProcess m_process;

  // True if the program name has been set for 'm_process'.
  bool m_hasProgramName;

  // True if we closed the write channel in 'm_process'.
  bool m_closedWriteChannel;

  // True if we already did 'm_process.kill()'.
  bool m_killedProcess;

  // Data to be fed to the process on standard input.
  QByteArray m_inputData;

  // Number of bytes from 'm_input' that have been written to the
  // child process.
  int m_bytesWritten;

  // Collected data that the process has written to its standard output.
  // This grows over time as more data is written.
  QByteArray m_outputData;

  // And what it has written to standard output.
  QByteArray m_errorData;

  // True once 'start' has been invoked.  This is used to prevent
  // this object from being re-used to run a second command.  (I do not
  // want to spend the effort to build that feature since it is easy
  // for the client to just create another CommandRunner.)
  bool m_startInvoked;

  // True if the process failed to complete and produce an exit code.
  // 'm_processError' and 'm_errorMessage' have more detail.
  bool m_failed;

  // If non-empty, a human-readable description of what went wrong.
  QString m_errorMessage;

  // How the process failed, if 'm_failed'.
  QProcess::ProcessError m_processError;

  // If not 'm_failed', this has the exit code.
  int m_exitCode;

protected:
  // Set m_failed to true with the given reasons and stop the event
  // loop.  But if m_failed is already true, disregard.
  void setFailed(QProcess::ProcessError pe, QString const &msg);

  // Stop the event loop with 'code' if it is running.
  void stopEventLoop(int code);

  // Kill the process if we haven't done so already.
  void killProcess();

  // Send some data to the process on its input channel.
  void sendData();

  // Called when the timer expires.
  virtual void timerEvent(QTimerEvent *event) override;

protected Q_SLOTS:
  // Handlers for signals emitted by QProcess.
  void on_errorOccurred(QProcess::ProcessError error);
  void on_finished(int exitCode, QProcess::ExitStatus exitStatus);
  void on_readyReadStandardError();
  void on_readyReadStandardOutput();
  void on_started();
  void on_stateChanged(QProcess::ProcessState newState);
  void on_aboutToClose();
  void on_bytesWritten(qint64 bytes);
  void on_channelBytesWritten(int channel, qint64 bytes);
  void on_channelReadyRead(int channel);
  void on_readChannelFinished();
  void on_readyRead();

public:      // funcs
  CommandRunner();
  virtual ~CommandRunner();

  // Specify the program to run.  This is required before invoking
  // 'start'.  If this is does not contain any path separators, it will
  // be looked up in the PATH environment variable.
  //
  // EXCEPTION: On Windows, the search order is:
  //   1. "The directory from which the application loaded."  I do not
  //      understand this one; the application has not loaded yet.
  //   2. The current directory.
  //   3. GetSystemDirectory(), e.g., c:/Windows/System32, which has a
  //      version of 'sort' among other things.  Beware!
  //   4. 16-bit system directory, e.g., c:/Windows/System.
  //   5. GetWindowsDirectory(), e.g., c:/Windows.
  //   6. PATH.
  // See MSDN docs for CreateProcess.
  void setProgram(QString const &program);

  // Specify the command line arguments to pass to the program.  The
  // default is to pass no arguments.
  void setArguments(QStringList const &arguments);

  // Specify the environment variable bindings to pass to the new
  // process.  The default is to pass those of the current process.
  // 'env' must not be empty.
  void setEnvironment(QProcessEnvironment const &env);

  // Specify what to pass on standard input.  Default is nothing.
  void setInputData(QByteArray const &data);

  // Run the program and wait for the process to exit.  Afterward, call
  // 'getFailed', etc., to see what happened.  This can only be run one
  // time for a given CommandRunner object.
  void startAndWait();

  // Get the content the process wrote to standard output.
  QByteArray const &getOutputData() const { return m_outputData; }

  // Similar for standard error.
  QByteArray const &getErrorData() const { return m_errorData; }

  // Process disposition getters.  See comments on respective fields.
  bool getFailed() const
    { return m_failed; }
  QString const &getErrorMessage() const
    { xassert(m_failed); return m_errorMessage; }
  QProcess::ProcessError getProcessError() const
    { xassert(m_failed); return m_processError; }
  int getExitCode() const
    { xassert(!m_failed); return m_exitCode; }
};


#endif // COMMAND_RUNNER_H
