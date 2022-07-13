// command-runner.h
// Declare the CommandRunner class.

#ifndef COMMAND_RUNNER_H
#define COMMAND_RUNNER_H

#include "refct-serf.h"                // SerfRefCount
#include "sm-override.h"               // OVERRIDE
#include "xassert.h"                   // xassert

#include <QByteArray>
#include <QEventLoop>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>


// Run a command on some input, gather the output.
class CommandRunner : public QObject, public SerfRefCount {
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

  // True if the 'startAndWait' was used to start the process.  In this
  // case, we need to exit the event loop when the process terminates.
  bool m_synchronous;

  // Queue of data to be fed to the process on standard input.  The
  // bytes still in this array have not yet been sent.
  QByteArray m_inputData;

  // Number of bytes from 'm_input' that have been written to the
  // child process.
  int m_bytesWritten;

  // Collected data that the process has written to its standard output.
  // This grows over time as more data is written.  It is emptied by
  // 'takeOutputData()'.
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

  // True once the body of CommandRunner::~CommandRunner() finishes.
  bool m_thisObjectDestroyed;

  // If non-empty, a human-readable description of what went wrong.
  QString m_errorMessage;

  // How the process failed, if 'm_failed'.
  QProcess::ProcessError m_processError;

  // If not 'm_failed', this has the exit code.
  int m_exitCode;

protected:   // funcs
  // Set m_failed to true with the given reasons and stop the event
  // loop.  But if m_failed is already true, disregard.
  void setFailed(QProcess::ProcessError pe, QString const &msg);

  // Stop the event loop with 'code' if it is running.
  void stopEventLoop(int code);

  // Called when the timer expires.
  virtual void timerEvent(QTimerEvent *event) OVERRIDE;

  // Send some data to the process on its input channel.
  void sendData();

public:      // funcs
  CommandRunner();
  virtual ~CommandRunner();

  // --------------------- starting the process ----------------------
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

  // Get the current program and arguments as a space-separated string.
  // This is meant for testing; it doesn't do any quoting.
  QString getCommandLine() const;

  // Set the program and arguments in order to invoke 'command' as a
  // POSIX shell command.  This will be "sh -c <command>" if it has any
  // shell metacharacters, or if 'alwaysUseSH', which is now the default
  // because on Windows, only 'sh' knows how to invoke shell scripts.
  void setShellCommandLine(QString const &command, bool alwaysUseSH = true);

  // Specify the environment variable bindings to pass to the new
  // process.  The default is to pass those of the current process.
  // 'env' must not be empty.
  void setEnvironment(QProcessEnvironment const &env);

  // Specify the directory in which to start the process.  The default
  // is the current directory of the parent.
  void setWorkingDirectory(QString const &dir);

  // Arrange to connect the child's stdin, stdout, and stderr to those
  // of the parent, rather than connecting pipes to them.  In this mode,
  // 'setInputData', etc., do not do anything.
  void forwardChannels();

  // ------------------- synchronous interface ---------------------
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

  // ------------------- process exit result -----------------------
  // Process disposition getters.  See comments on respective fields.
  bool getFailed() const
    { return m_failed; }
  QString const &getErrorMessage() const
    { xassert(m_failed); return m_errorMessage; }
  QProcess::ProcessError getProcessError() const
    { xassert(m_failed); return m_processError; }
  int getExitCode() const
    { xassert(!m_failed); return m_exitCode; }

  // If the process exited normally, return "Exited with code N.", where
  // N is the exit code.  If it exited abnormally, return
  // 'getErrorMessage()'.  If it has not terminated, return "Not
  // terminated."
  QString getTerminationDescription() const;

  // ------------------- asynchronous interface --------------------
  // Start the process and return immediately while it runs in the
  // background.
  void startAsynchronous();

  // Write some data to the child's standard input.  This cannot be
  // called until after calling 'startAsynchronous'.
  //
  // It seems that Qt will buffer an arbitrarily large amount of input
  // data with no way to tell whether the process has consumed it.
  void putInputData(QByteArray const &input);

  // Close the standard input channel.  Once this is called, no more
  // data should be passed to 'putInputData'.  Any data already
  // queued will be sent to the process before the channel is closed.
  void closeInputChannel();

  // True if the child has written some data to its standard output.
  bool hasOutputData() const;

  // Get that data, destructively removing it from the output queue.  If
  // this is called while 'hasOutputData()' is false, it will simply
  // return an empty array.
  QByteArray takeOutputData();

  // True if there is stderr data.
  bool hasErrorData() const;

  // Get the stderr data.
  QByteArray takeErrorData();

  // Kill the process if we haven't done so already.  If there is a
  // problem, including that we already tried to kill it, return a
  // string describing it; otherwise "".  This will wait up to half a
  // second for the process to terminate, during which the event queue
  // is *not* pumped, so the app freezes.
  QString killProcess();

  // Attempt to kill the process without waiting for it afterward.  It
  // is best to avoid doing this and then immediately destroying the
  // CommandRunner object because the QProcess will be confused.
  QString killProcessNoWait();

  // ---------------------- process status -------------------------
  // Return true if the process has started and not terminated.  Only
  // once it terminates are the process exit result functions
  // meaningful.
  bool isRunning() const;

  // ------------------ line-oriented output -----------------------
  // The methods in this section provide a line-oriented interface to
  // the output and error data.  They assume that the child process is
  // using the UTF-8 character encoding.

  // True when there is at least one newline in m_outputData.
  bool hasOutputLine() const;

  // Retrieve the next complete line of data in m_outputData, terminated
  // by a newline character.  If there is no newline in m_outputData,
  // then return what there is, *without* a newline terminator.  That
  // may be the empty string.
  QString getOutputLine();

  // True when there is at least one newline in m_errorData.
  bool hasErrorLine() const;

  // Get the next line with newline, or fragment without.
  QString getErrorLine();

  // ------------------------- signals -----------------------------
Q_SIGNALS:
  // Emitted when 'hasOutputLine()' becomes true.
  void signal_outputLineReady();

  // Emitted when 'hasOutputData()' becomes true.
  void signal_outputDataReady();

  // Emitted when 'hasErrorLine()' becomes true.
  void signal_errorLineReady();

  // Emitted when 'hasErrorData()' becomes true.
  void signal_errorDataReady();

  // Emitted when 'isRunning()' becomes false.
  void signal_processTerminated();

  // -------------------------- slots ------------------------------
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
};


// Return a string interpretation of a ProcessError code.
char const *toString(QProcess::ProcessError error);


#endif // COMMAND_RUNNER_H
