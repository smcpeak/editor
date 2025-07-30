// command-runner.cc
// code for command-runner.h

#include "command-runner.h"            // this module

// smqtutil
#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtutil.h"           // waitForQtEvent

// smbase
#include "smbase/codepoint.h"          // isShellMetacharacter
#include "smbase/sm-iostream.h"        // cerr, etc.
#include "smbase/trace.h"              // TRACE
#include "smbase/xassert.h"            // xassertPrecondition, xassert

// Qt
#include <qtcoreversion.h>             // QTCORE_VERSION
#include <Qt>                          // Qt::SkipEmptyParts
#include <QTimerEvent>

// libc++
#include <algorithm>                   // std::min
#include <utility>                     // std::move

// libc
#include <assert.h>                    // assert


// Tracing for this module.
#define TRACE_CR(msg) TRACE("cmdrun", msg) /* user ; */

// Additional level of detail.
#define TRACE_CR_DETAIL(msg) TRACE("cmdrun_detail", msg) /* user ; */


#if QTCORE_VERSION < 0x50E00
  // Qt::SkipEmptyParts was introduced in Qt 5.14 as a replacement for
  // QString::SkipEmptyParts.
  #error "This program requires Qt 5.14 or later."
#endif


// Maximum time for the synchronous runner invocation.
int const CommandRunner::DEFAULT_SYNCHRONOUS_TIME_LIMIT_MS = 2000;

// How long to wait after trying to kill a process.
static int const KILL_WAIT_TIMEOUT_MS = 500;


static char const *toString(QProcess::ProcessState state)
{
  switch (state) {
    case QProcess::NotRunning:   return "NotRunning";
    case QProcess::Starting:     return "Starting";
    case QProcess::Running:      return "Running";
    default:                     return "Invalid ProcessState";
  }
}


CommandRunner::CommandRunner()
  : m_eventLoop(),
    m_timerId(-1),
    m_process(),
    m_hasProgramName(false),
    m_closedWriteChannel(false),
    m_killedProcess(false),
    m_synchronous(false),
    m_inputData(),
    m_bytesWritten(0),
    m_outputData(),
    m_errorData(),
    m_startInvoked(false),
    m_failed(false),
    m_thisObjectDestroyed(false),
    m_errorMessage(),
    m_processError(QProcess::UnknownError),
    m_exitCode(-1),
    m_synchronousTimeLimitMS(DEFAULT_SYNCHRONOUS_TIME_LIMIT_MS)
{
  // Some macros to make the connection process a bit more clear.
  // I might move these to smqtutil at some point (after appropriately
  // generalizing them).
  #define CONN(sig)                                       \
    QObject::connect(&m_process, &QProcess::sig,          \
                     this, &CommandRunner::on_##sig) /* user ; */

  #define CONN_OV(sig, paramTypes)                          \
  {                                                         \
    void (QProcess::*ptm) paramTypes = &QProcess::sig;      \
    QObject::connect(&m_process, ptm,                       \
                     this, &CommandRunner::on_##sig);       \
  }

  CONN(errorOccurred);
  CONN_OV(finished, (int, QProcess::ExitStatus));
  CONN(readyReadStandardError);
  CONN(readyReadStandardOutput);
  CONN(started);
  CONN(stateChanged);
  CONN(aboutToClose);
  CONN(bytesWritten);
  CONN(channelBytesWritten);
  CONN(channelReadyRead);
  CONN(readChannelFinished);
  CONN(readyRead);

  #undef CONN_OV
  #undef CONN
}

CommandRunner::~CommandRunner()
{
  TRACE_CR("~CommandRunner: state=" << toString(m_process.state()));

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_process, NULL, this, NULL);

  if (m_killedProcess && m_process.state() != QProcess::NotRunning) {
    // We already tried and failed to kill the process.  The QProcess
    // destructor will try again, but most likely also fail, then
    // hang for 30s.  See doc/qprocess-hangs.txt.
    QStringList args(m_process.arguments());
    cerr << "Warning: The command \"" << toString(m_process.program())
         << (args.isEmpty()? "" : " ") << toString(args.join(' '))
         << "\", with process ID " << m_process.processId()
         << ", is still running, despite a prior attempt to kill it.  "
            "Most likely, this will cause a 30s hang, during which no "
            "interaction is possible, due to a limitation in Qt."
         << endl;
  }

  // After this line, the members of CommandRunner are destroyed,
  // including 'm_process'.  That member's destructor does quite a bit,
  // including sending signals.  I want to be able to verify that my
  // methods are not being further invoked, since that risks memory
  // corruption.
  m_thisObjectDestroyed = true;
}


void CommandRunner::setFailed(QProcess::ProcessError pe, QString const &msg)
{
  TRACE_CR("setFailed: pe=" << pe << ", msg: " << toString(msg));

  if (!m_failed) {
    m_failed = true;
    m_processError = pe;
    m_errorMessage = msg;

    this->stopEventLoop(10);
  }
  else {
    // We already have an error message.  Discard subsequent messages
    // because they may arise from various signals sent as the process
    // running infrastructure shuts down, but only the first message
    // arose directly from whatever happened.
    TRACE_CR("setFailed: disregarding due to prior message");
  }
}


void CommandRunner::stopEventLoop(int code)
{
  if (m_synchronous) {
    if (m_eventLoop.isRunning()) {
      TRACE_CR("stopEventLoop: terminating event loop");
      m_eventLoop.exit(code);
    }
    else {
      TRACE_CR("stopEventLoop: event loop is not running");
    }
  }
  else {
    TRACE_CR("stopEventLoop: not in synchronous mode, ignoring");
  }
}


void CommandRunner::timerEvent(QTimerEvent *event)
{
  TRACE_CR("timerEvent");

  this->killTimer(m_timerId);
  m_timerId = -1;

  this->setFailed(QProcess::Timedout,
    qstringb("Timed out after " << m_synchronousTimeLimitMS << " ms."));

  TRACE_CR("timerEvent: killing process");
  this->killProcess();
}


QString CommandRunner::killProcessNoWait()
{
  if (m_killedProcess) {
    TRACE_CR("killProcess: not killing process again");
    return "Already attempted to kill process.";
  }

  if (m_process.state() == QProcess::NotRunning) {
    TRACE_CR("killProcess: not killing process since it is not running");
    return "Process is not running.";
  }

  // Remember that we tried to kill already.
  m_killedProcess = true;

  // There is unfortunately no way to get any OS errors from this call.
  // That is a limitation of QProcess.
  TRACE_CR("killProcess: calling QProcess::kill");
  m_process.kill();        // This returns immediately.
  return "";
}


QString CommandRunner::killProcess()
{
  QString ret = this->killProcessNoWait();
  if (!ret.isEmpty()) {
    return ret;
  }

  // The 'wait' call blocks without pumping the event queue, so the
  // app freezes if this takes time.
  TRACE_CR("killProcess: waitForFinished");
  if (m_process.waitForFinished(KILL_WAIT_TIMEOUT_MS)) {
    TRACE_CR("killProcess: waitForFinished returned true");
    return "";
  }
  else {
    // This is somewhat bad because, at the very least, m_process is
    // going to complain (to stderr) when it is destroyed but the child
    // has not died yet.
    TRACE_CR("killProcess: waitForFinished returned false");
    return qstringb("Process did not die after " << KILL_WAIT_TIMEOUT_MS <<
                    " milliseconds.  I don't know why.");
  }
}


// ---------------------- starting the process -----------------------
void CommandRunner::setProgram(QString const &program)
{
  xassertPrecondition(!m_startInvoked);

  m_process.setProgram(program);
  m_hasProgramName = true;
}


void CommandRunner::setArguments(QStringList const &arguments)
{
  xassertPrecondition(!m_startInvoked);

  m_process.setArguments(arguments);
}


QString CommandRunner::getCommandLine() const
{
  QString ret(m_process.program());

  QStringList args(m_process.arguments());
  if (!args.isEmpty()) {
    ret.append(' ');
    ret.append(args.join(' '));
  }

  return ret;
}


// True if any character in 'c' is a shell metacharacter other than
// space.
//
// This does not look for shell reserved words such as "if", but for
// those to work as intended, some other metacharacter (mainly ';')
// must also be present, so that shouldn't cause problems.
static bool hasShellMetacharacters(QString const &s)
{
  for (int i=0; i < s.length(); i++) {
    int c = s[i].unicode();
    if (c != ' ' && isShellMetacharacter(c)) {
      return true;
    }
  }
  return false;
}

// The idea with this function was if I do not use 'sh', then I can
// kill the child process directly.  Otherwise, I can only kill the
// shell and just hope the child decides to terminate too.
//
// The problem with that is, on Windows, only 'sh' knows how to invoke
// shell scripts.  Also, the search algorithm of CreateProcess is wonky,
// implicitly putting C:/Windows/System32 (which has sort.exe, among
// others) ahead of the first element of PATH.  It's possible there are
// similar subtle differences on unix between execvp and 'sh', which
// would lead to inconsistency in behavior depending on the presence of
// shell metacharacters.  So at least for now I've arranged it so
// 'alwaysUseSH' is always true (outside of test code).
//
// TODO: I'd like to build out proper job management where I can, for
// example, kill an entire process group.  But I'm not sure how much
// effort that is, especially on Windows.
void CommandRunner::setShellCommandLine(QString const &command,
                                        bool alwaysUseSH)
{
  if (alwaysUseSH || hasShellMetacharacters(command)) {
    this->setProgram("sh");
    this->setArguments(QStringList() << "-c" << command);
  }
  else {
    // Split directly.
    QStringList words(command.split(' ', Qt::SkipEmptyParts));
    if (!words.isEmpty()) {
      this->setProgram(words.first());
      words.removeFirst();
      this->setArguments(words);
    }
    else {
      // Rather than call this an error, just use the shell to invoke
      // this program name consisting entirely of whitespace.
      this->setProgram("sh");
      this->setArguments(QStringList() << "-c" << command);
    }
  }
}


void CommandRunner::setEnvironment(QProcessEnvironment const &env)
{
  xassertPrecondition(!m_startInvoked);

  // The issue here is QProcess will silently ignore any specified
  // environment that is completely empty (since, internally, that
  // is how it represents "no specified environment"), so I want to
  // prohibit it in my interface.
  xassertPrecondition(!env.isEmpty());

  m_process.setProcessEnvironment(env);
}


void CommandRunner::setWorkingDirectory(QString const &dir)
{
  xassertPrecondition(!m_startInvoked);

  m_process.setWorkingDirectory(dir);
}


void CommandRunner::forwardChannels()
{
  xassertPrecondition(!m_startInvoked);

  m_process.setInputChannelMode(QProcess::ForwardedInputChannel);
  m_process.setProcessChannelMode(QProcess::ForwardedChannels);
}


void CommandRunner::mergeStderrIntoStdout()
{
  xassertPrecondition(!m_startInvoked);

  m_process.setProcessChannelMode(QProcess::MergedChannels);
}


void CommandRunner::setStandardErrorFile(QString const &path)
{
  xassertPrecondition(!m_startInvoked);

  m_process.setStandardErrorFile(path);
}


// -------------------- synchronous interface ----------------------
void CommandRunner::setInputData(QByteArray const &data)
{
  xassertPrecondition(!m_startInvoked);

  // QByteArray internally shares data, so this is constant-time.
  m_inputData = data;
}


void CommandRunner::startAndWait()
{
  // The program name must have been set.
  xassertPrecondition(m_hasProgramName);

  // Client should not have caused a problem yet.
  xassertPrecondition(!m_failed);

  TRACE_CR("startAndWait: cmd: " << toString(m_process.program()));
  if (!m_process.arguments().isEmpty()) {
    TRACE_CR("startAndWait: args: " << toString(m_process.arguments().join(' ')));
  }

  // This function can only be used once per object.
  xassertPrecondition(!m_startInvoked);
  m_startInvoked = true;
  m_synchronous = true;

  // Start the timer.
  m_timerId = this->startTimer(m_synchronousTimeLimitMS);

  // Begin running the child process.
  m_process.start();

  if (m_failed) {
    TRACE_CR("startAndWait: process could not start");
    return;
  }

  // Prime the event loop by sending some data to the process.
  this->sendData();

  if (m_failed) {
    TRACE_CR("startAndWait: failure in first sendData");
    return;
  }

  // Drop into the event loop to wait for process events.  This
  // returns when we are finished working with the process, either
  // because it completed successfully or we encountered an error.
  TRACE_CR("startAndWait: starting event loop");
  int r = m_eventLoop.exec();

  // The return from exec() is not supposed to be important because I
  // should have already recorded all the relevant information in the
  // data members, but I'll log it at least.
  TRACE_CR("startAndWait: event loop terminated with code " << r);
}


void CommandRunner::sendData()
{
  // QProcess::write will accept arbitrarily large amounts of data
  // at a time, but I want to limit it in order to better exercise
  // the mechanism that reads in chunks.
  int maxLen = std::min(m_inputData.size(), 0x2000);

  int len = m_process.write(m_inputData.constData(), maxLen);
  TRACE_CR("sendData: write(written=" << m_bytesWritten <<
           ", maxLen=" << maxLen << "): len=" << len);

  if (len < 0) {
    // There does not seem to be a documented way to get more
    // information about the error here, such as the errno value.
    this->setFailed(QProcess::WriteError,
      qstringb("Error while writing to process standard input (b=" <<
               m_bytesWritten << ", m=" << maxLen << ")."));
    TRACE_CR("sendData: killing process");
    this->killProcess();
  }

  else {
    xassert(len <= m_inputData.size());
    m_bytesWritten += len;

    // Remove the sent data from m_inputData.
    m_inputData.remove(0, len);

    if (m_synchronous) {
      // Check for being done writing.
      if (m_inputData.isEmpty()) {
        this->closeInputChannel();
      }
    }
  }
}


QString CommandRunner::getTerminationDescription() const
{
  if (isRunning()) {
    return "Not terminated.";
  }

  if (getFailed()) {
    return getErrorMessage();
  }
  else {
    return qstringb("Exited with code " << getExitCode() << ".");
  }
}


// -------------------- asynchronous interface ---------------------
void CommandRunner::startAsynchronous()
{
  // The program name must have been set.
  xassertPrecondition(m_hasProgramName);

  // The client should not already have done anything that triggers
  // the failure flag to be set, otherwise we'll get confused about
  // detecting process termination.
  xassertPrecondition(!m_failed);

  TRACE_CR("startAsync: cmd: " << toString(m_process.program()));
  if (!m_process.arguments().isEmpty()) {
    TRACE_CR("startAsync: args: " << toString(m_process.arguments().join(' ')));
  }

  // This function can only be used once per object.
  xassertPrecondition(!m_startInvoked);
  m_startInvoked = true;
  xassert(m_synchronous == false); // Should still have its initial value.

  // Begin running the child process.
  m_process.start();

  // NOTE: It is not safe to check, for example, 'm_failed' here.  Even
  // for the case of attempting to invoke a program that does not exist,
  // it may or may not be set here.  Instead, one must wait for QProcess
  // to send a signal in order to determine the process' fate.

  // If some data has already been submitted by the client, send it to
  // the child process.
  if (!m_inputData.isEmpty()) {
    this->sendData();
  }
}


bool CommandRunner::waitForStarted(int msecs)
{
  return m_process.waitForStarted(msecs);
}


void CommandRunner::putInputData(QByteArray const &input)
{
  // You can't start putting data until the process is started.
  xassertPrecondition(m_startInvoked);

  m_inputData.append(input);
  this->sendData();
}


void CommandRunner::closeInputChannel()
{
  // You can't close the input channel until the process is started.
  xassertPrecondition(m_startInvoked);

  if (m_closedWriteChannel) {
    // I do not know that it would be a problem to close it more
    // than once, but that seems inelegant.
    TRACE_CR("closeInputChannel: write channel closed already");
  }
  else {
    TRACE_CR("closeInputChannel: closing write channel");
    m_process.closeWriteChannel();
    m_closedWriteChannel = true;
  }
}


bool CommandRunner::outputChannelOpen()
{
  // For some reason, when the process terminates, the output channel is
  // still reported as open, so I have to explicitly check for
  // termination.
  //
  // I spent a while digging through the sources, and I think the basic
  // problem is that `QProcess` itself does not properly distinguish
  // between an output channel closing and the child process
  // terminating.
  //
  if (!isRunning()) {
    return false;
  }

  // `QProcess` exposes two output channels, but you have to switch
  // between them statefully, meaning this method cannot be `const`.
  // It's weird.
  m_process.setReadChannel(QProcess::StandardOutput);

  // Ask whether the output ("read") channel we just activated is open.
  if (!m_process.isOpen()) {
    return false;
  }

  // Maybe this works?  No.
  //return !m_process.atEnd();
  return true;
}


void CommandRunner::waitForOutputChannelClosed()
{
  TRACE_CR("waitForOutputChannelClosed: start");
  while (outputChannelOpen()) {
    waitForQtEvent();
  }
  TRACE_CR("waitForOutputChannelClosed: end");
}


bool CommandRunner::hasOutputData() const
{
  return !m_outputData.isEmpty();
}


bool CommandRunner::hasSizedOutputData(int size) const
{
  return peekOutputData().size() >= size;
}


QByteArray CommandRunner::takeOutputData()
{
  QByteArray ret(std::move(m_outputData));
  m_outputData.clear();
  return ret;
}


QByteArray const &CommandRunner::peekOutputData() const
{
  return m_outputData;
}


void CommandRunner::removeOutputData(int size)
{
  xassert(size >= 0);
  m_outputData.remove(0, size);
}


QByteArray CommandRunner::takeSizedOutputData(int size)
{
  QByteArray res(m_outputData.left(size));
  removeOutputData(size);
  return res;
}


bool CommandRunner::hasErrorData() const
{
  return !m_errorData.isEmpty();
}


QByteArray CommandRunner::takeErrorData()
{
  QByteArray ret(m_errorData);
  m_errorData.clear();
  return ret;
}


// ----------------------- process status --------------------------
bool CommandRunner::isRunning() const
{
  // From my perspective, QProcess::Starting and QProcess::Running are
  // the same.
  return (m_process.state() != QProcess::NotRunning);
}


void CommandRunner::waitForNotRunning()
{
  TRACE_CR("waitForNotRunning: start");
  while (isRunning()) {
    waitForQtEvent();
  }
  TRACE_CR("waitForNotRunning: end");
}


qint64 CommandRunner::getChildPID() const
{
  return m_process.processId();
}


// -------------------- line-oriented output -----------------------
static int findUtf8Newline(QByteArray const &arr)
{
  return arr.indexOf('\n');
}

static bool hasUtf8Newline(QByteArray const &arr)
{
  return findUtf8Newline(arr) >= 0;
}

static QString extractUtf8Line(QByteArray &arr)
{
  int i = findUtf8Newline(arr);

  int bytesToRemove = (i>=0 ? i+1 : arr.size());
  QByteArray utf8Line = arr.left(bytesToRemove);
  arr.remove(0, bytesToRemove);
  return QString::fromUtf8(utf8Line);
}


bool CommandRunner::hasOutputLine() const
{
  return hasUtf8Newline(m_outputData);
}


QString CommandRunner::getOutputLine()
{
  return extractUtf8Line(m_outputData);
}


QString CommandRunner::waitForOutputLine()
{
  TRACE_CR("waitForOutputLine: start");
  while (!hasOutputLine() && outputChannelOpen()) {
    TRACE_CR_DETAIL("waitForOutputLine:"
      " hasOutputLine=" << hasOutputLine() <<
      " outputChannelOpen=" << outputChannelOpen());
    waitForQtEvent();
  }
  TRACE_CR("waitForOutputLine: end");

  return getOutputLine();
}


QByteArray CommandRunner::waitForOutputData(int size)
{
  TRACE_CR("waitForOutputData: start");
  while (m_outputData.size() < size && outputChannelOpen()) {
    waitForQtEvent();
  }
  TRACE_CR("waitForOutputData: end");

  // Grab the first `size` bytes.
  QByteArray ret(m_outputData.constData(), size);

  // Remove those from `m_outputData`.
  m_outputData.remove(0, size);

  return ret;
}


bool CommandRunner::hasErrorLine() const
{
  return hasUtf8Newline(m_errorData);
}


QString CommandRunner::getErrorLine()
{
  return extractUtf8Line(m_errorData);
}


// ---------------------------- slots ------------------------------
char const *toString(QProcess::ProcessError error)
{
  switch (error) {
    case QProcess::FailedToStart:
      // An unfortunate aspect of QProcess is its lumping together
      // several reasons under this code.
      return "Failed to start process; possible reasons include (but "
             "are not limited to) a missing executable, permission "
             "error, and an invalid starting directory.";

    case QProcess::Crashed:
      // In my experiments on Windows, calling m_process.kill() causes
      // 'on_errorOccurred' to be called with QProcess::Crashed, hence
      // the "or was killed" portion of this string.
      return "Process crashed or was killed";

    case QProcess::Timedout:
      // Hopefully this message never propagates to the user because
      // it does not specify the timeout value.  (I do not want to
      // just assume it is DEFAULT_SYNCHRONOUS_TIME_LIMIT_MS here.)
      return "Process ran for longer than its timeout period";

    case QProcess::WriteError:
      return "Error writing to the process' input";

    case QProcess::ReadError:
      return "Error reading from the process' output";

    case QProcess::UnknownError:
    default:
      return "Error with unknown cause";
  }
}


void CommandRunner::on_errorOccurred(QProcess::ProcessError error)
{
  char const *errorString = toString(error);
  TRACE_CR("on_errorOccurred: e=" << error << ", str: " << errorString);
  assert(!m_thisObjectDestroyed);

  this->setFailed(error, errorString);

  if (error == QProcess::FailedToStart) {
    // The client is expecting 'processTerminated' at some point, and
    // that normally happens in 'on_finished'.  But if the process fails
    // to start in the first place, 'on_finished' is never called, so
    // emit the expected signal here.
    TRACE_CR("on_errorOccurred: emitting signal_processTerminated");
    Q_EMIT signal_processTerminated();
  }

  TRACE_CR("on_errorOccurred: killing process");
  this->killProcess();
}


static char const *toString(QProcess::ExitStatus s)
{
  switch (s) {
    case QProcess::NormalExit:     return "NormalExit";
    case QProcess::CrashExit:      return "CrashExit";
    default:                       return "Invalid ExitStatus";
  }
}

void CommandRunner::on_finished(int exitCode, QProcess::ExitStatus exitStatus)
{
  TRACE_CR("on_finished: exitCode=" << exitCode <<
           ", status=" << toString(exitStatus));
  assert(!m_thisObjectDestroyed);

  if (exitStatus == QProcess::CrashExit) {
    this->setFailed(QProcess::Crashed, toString(QProcess::Crashed));
  }
  else {
    m_exitCode = exitCode;

    TRACE_CR("on_finished: calling stopEventLoop");
    this->stopEventLoop(0);
  }

  TRACE_CR("on_finished: emitting signal_processTerminated");
  Q_EMIT signal_processTerminated();
}


void CommandRunner::on_readyReadStandardError()
{
  // Redundant with 'channelReadyRead'.
  TRACE_CR("on_readyReadStandardError");
}


void CommandRunner::on_readyReadStandardOutput()
{
  // Redundant with 'channelReadyRead'.
  TRACE_CR("on_readyReadStandardOutput");
}


void CommandRunner::on_started()
{
  TRACE_CR_DETAIL("on_started");
}


void CommandRunner::on_stateChanged(QProcess::ProcessState newState)
{
  TRACE_CR("on_stateChanged: " << toString(newState));

  if (newState == QProcess::NotRunning) {
    // We want to stop, but on_finished should get called as well, and
    // that has the exit code.  So, here, I will just do nothing, and
    // let on_finished terminate the event loop.
  }
}


void CommandRunner::on_aboutToClose()
{
  TRACE_CR("on_aboutToClose");
}


void CommandRunner::on_bytesWritten(qint64 bytes)
{
  TRACE_CR("on_bytesWritten: " << bytes);
  assert(!m_thisObjectDestroyed);
  this->sendData();
}


void CommandRunner::on_channelBytesWritten(int channel, qint64 bytes)
{
  // So far, I have never seen this method get called.
  TRACE_CR("on_channelBytesWritten: c=" << channel << " b=" << bytes);
}


// Append 'buf/len' to 'arr'.  Return true if 'arr' originally did not
// have a UTF-8 newline and afterward does.
static bool appendData_gainedUtf8Newline(QByteArray &arr,
                                         char const *buf, int len)
{
  bool before = hasUtf8Newline(arr);
  arr.append(buf, len);
  if (!before) {
    return hasUtf8Newline(arr);
  }
  else {
    return false;
  }
}


void CommandRunner::on_channelReadyRead(int channelNumber)
{
  TRACE_CR("on_channelReadyRead: " << channelNumber);
  assert(!m_thisObjectDestroyed);

  // Decode 'channelNumber'.
  QProcess::ProcessChannel channel;
  char const *channelName;
  switch (channelNumber) {
    case QProcess::StandardOutput:
      channel = QProcess::StandardOutput;
      channelName = "standard output";
      break;

    case QProcess::StandardError:
      channel = QProcess::StandardError;
      channelName = "standard error";
      break;

    default:
      xfailure("invalid channel number");
      return;
  }

  m_process.setReadChannel(channel);
  TRACE_CR("on_channelReadyRead: avail=" << m_process.bytesAvailable());

  while (m_process.bytesAvailable()) {
    // I would like to be reading this in chunks, but Qt internally
    // buffers an enormous amount of data (100MB+).  I tried adding
    // Unbuffered to the 'start' call but that had no effect.
    char buf[0x2000];
    int len = m_process.read(buf, 0x2000);

    if (len < 0) {
      this->setFailed(QProcess::ReadError,
        qstringb("Error while reading from process " << channelName <<
                 " (b=" << m_outputData.size() << ")."));
      TRACE_CR("on_channelReadyRead: killing process");
      this->killProcess();
    }

    else if (len == 0) {
      TRACE_CR("on_channelReadyRead: hit EOF");

      // I assume that I will not receive any more notifications of
      // this kind, and hence will not try to read past EOF.
    }

    else {
      TRACE_CR("on_channelReadyRead: got " << len << " bytes");
      if (channel == QProcess::StandardOutput) {
        if (appendData_gainedUtf8Newline(m_outputData, buf, len)) {
          TRACE_CR("emitting signal_outputLineReady");
          Q_EMIT signal_outputLineReady();
        }
        Q_EMIT signal_outputDataReady();
      }
      else {
        if (appendData_gainedUtf8Newline(m_errorData, buf, len)) {
          TRACE_CR("emitting signal_errorLineReady");
          Q_EMIT signal_errorLineReady();
        }
        Q_EMIT signal_errorDataReady();
      }
    }
  }
}


void CommandRunner::on_readChannelFinished()
{
  // This does not seem useful because I only get this for the
  // "current" read channel, which is either stdout or stderr but
  // not both.
  TRACE_CR_DETAIL("on_readChannelFinished");
}


void CommandRunner::on_readyRead()
{
  TRACE_CR_DETAIL("on_readyRead");
}


// EOF
