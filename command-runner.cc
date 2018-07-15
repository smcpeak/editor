// command-runner.cc
// code for command-runner.h

#include "command-runner.h"            // this module

// smqtutil
#include "qtutil.h"                    // qstringb

// smbase
#include "sm-iostream.h"               // cerr, etc.
#include "trace.h"                     // TRACE

// Qt
#include <qtcoreversion.h>             // QTCORE_VERSION
#include <QTimerEvent>

// libc
#include <assert.h>                    // assert


// Tracing for this module.
#define TRACE_CR(msg) TRACE("cmdrun", msg) /* user ; */

// Additional level of detail.
#define TRACE_CR_DETAIL(msg) TRACE("cmdrun_detail", msg) /* user ; */


// Maximum time for the synchronous runner invocation.
static int const TIME_LIMIT_MS = 2000;

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
    m_exitCode(-1)
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

#if QTCORE_VERSION < 0x50700
  // For example, I am using 'channelReadyRead', which was introduced
  // in Qt 5.7.
  #error "This program requires Qt 5.7 or later."
#endif

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
    qstringb("Timed out after " << TIME_LIMIT_MS << " ms."));

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
  xassert(!m_startInvoked);

  m_process.setProgram(program);
  m_hasProgramName = true;
}


void CommandRunner::setArguments(QStringList const &arguments)
{
  xassert(!m_startInvoked);

  m_process.setArguments(arguments);
}


void CommandRunner::setEnvironment(QProcessEnvironment const &env)
{
  xassert(!m_startInvoked);

  // The issue here is QProcess will silently ignore any specified
  // environment that is completely empty (since, internally, that
  // is how it represents "no specified environment"), so I want to
  // prohibit it in my interface.
  xassert(!env.isEmpty());

  m_process.setProcessEnvironment(env);
}


void CommandRunner::setWorkingDirectory(QString const &dir)
{
  xassert(!m_startInvoked);

  m_process.setWorkingDirectory(dir);
}


void CommandRunner::forwardChannels()
{
  m_process.setInputChannelMode(QProcess::ForwardedInputChannel);
  m_process.setProcessChannelMode(QProcess::ForwardedChannels);
}


// -------------------- synchronous interface ----------------------
void CommandRunner::setInputData(QByteArray const &data)
{
  xassert(!m_startInvoked);

  // QByteArray internally shares data, so this is constant-time.
  m_inputData = data;
}


void CommandRunner::startAndWait()
{
  // The program name must have been set.
  xassert(m_hasProgramName);

  // Client should not have caused a problem yet.
  xassert(!m_failed);

  TRACE_CR("startAndWait: cmd: " << toString(m_process.program()));
  if (!m_process.arguments().isEmpty()) {
    TRACE_CR("startAndWait: args: " << toString(m_process.arguments().join(' ')));
  }

  // This function can only be used once per object.
  xassert(!m_startInvoked);
  m_startInvoked = true;
  m_synchronous = true;

  // Start the timer.
  m_timerId = this->startTimer(TIME_LIMIT_MS);

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
  int maxLen = min(m_inputData.size(), 0x2000);

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


// -------------------- asynchronous interface ---------------------
void CommandRunner::startAsynchronous()
{
  // The program name must have been set.
  xassert(m_hasProgramName);

  // The client should not already have done anything that triggers
  // the failure flag to be set, otherwise we'll get confused about
  // detecting process termination.
  xassert(!m_failed);

  TRACE_CR("startAsync: cmd: " << toString(m_process.program()));
  if (!m_process.arguments().isEmpty()) {
    TRACE_CR("startAsync: args: " << toString(m_process.arguments().join(' ')));
  }

  // This function can only be used once per object.
  xassert(!m_startInvoked);
  m_startInvoked = true;
  m_synchronous = false;     // Just for clarity; it already is false.

  // Begin running the child process.
  m_process.start();

  if (m_failed) {
    TRACE_CR("startAsync: process could not start");
    return;
  }

  // If some data has already been submitted by the client, send it to
  // the child process.
  if (!m_inputData.isEmpty()) {
    this->sendData();
  }
}


void CommandRunner::putInputData(QByteArray const &input)
{
  // You can't start putting data until the process is started.
  xassert(m_startInvoked);

  m_inputData.append(input);
  this->sendData();
}


void CommandRunner::closeInputChannel()
{
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


bool CommandRunner::hasOutputData() const
{
  return !m_outputData.isEmpty();
}


QByteArray CommandRunner::takeOutputData()
{
  QByteArray ret(m_outputData);
  m_outputData.clear();
  return ret;
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
      return "Failed to start process";

    case QProcess::Crashed:
      // In my experiments on Windows, calling m_process.kill() causes
      // 'on_errorOccurred' to be called with QProcess::Crashed, hence
      // the "or was killed" portion of this string.
      return "Process crashed or was killed";

    case QProcess::Timedout:
      // Hopefully this message never propagates to the user because
      // it does not specify the timeout value.  (I do not want to
      // just assume it is TIME_LIMIT_MS here.)
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

  TRACE_CR("emitting signal_processTerminated");
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
      }
      else {
        if (appendData_gainedUtf8Newline(m_errorData, buf, len)) {
          TRACE_CR("emitting signal_errorLineReady");
          Q_EMIT signal_errorLineReady();
        }
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
