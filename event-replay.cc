// event-replay.cc
// code for event-replay.h

#include "event-replay.h"              // this module

// smqtutil
#include "qtguiutil.h"                 // getKeyPressFromString
#include "qtutil.h"                    // operator<<(QString)

// smbase
#include "syserr.h"                    // xsyserror

// Qt
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QRegularExpression>
#include <QStringList>
#include <QWidget>

// libc++
#include <string>                      // std::string, getline


EventReplay::EventReplay(string const &fname, EventReplayQuery *query)
  : m_fname(fname),
    m_in(m_fname.c_str()),
    m_query(query)
{
  xassert(m_query);
  if (m_in.fail()) {
    xsyserror("open", m_fname.c_str());
  }
}


EventReplay::~EventReplay()
{}


// Throw a string built using 'stringb'.
#define xstringb(msg) throw stringb(msg).str() /* user ; */


static QWidget *getWidgetFromPath(string const &path)
{
  QStringList elts = toQString(path).split('.');
  if (elts.isEmpty()) {
    throw string("empty path");
  }

  try {
    Q_FOREACH (QWidget *widget, QApplication::topLevelWidgets()) {
      if (widget->objectName() == elts.at(0)) {
        for (int i=1; i < elts.count(); i++) {
          QString elt(elts.at(i));
          if (elt.isEmpty()) {
            xstringb("empty path element " << (i+1));
          }
          widget = widget->findChild<QWidget*>(elt,
                                               Qt::FindDirectChildrenOnly);
          if (!widget) {
            xstringb("could not find child \"" << elt <<
                     "\" at path element " << (i+1));
          }
        }
        return widget;
      }
    }

    xstringb("could not find root element \"" <<
             elts.at(0) << "\"");
  }
  catch (string const &msg) {
    xstringb("in path \"" << path << "\": " << msg);
  }
}


template <class T>
T *getQObjectFromPath(string const &path)
{
  QWidget *w = getWidgetFromPath(path);
  T *t = qobject_cast<T*>(w);
  if (!t) {
    xstringb("widget at \"" << path <<
             "\" is not " << T::staticMetaObject.className());
  }
  return t;
}


#define BIND_ARGS1(arg1)                              \
  if (numArgs != 1) {                                 \
    xstringb("incorrect number of arguments to " <<   \
             funcName << "; " << numArgs <<           \
             " passed but 1 required");               \
  }                                                   \
  string arg1 = toString(match.captured(2));


#define BIND_ARGS2(arg1, arg2)                        \
  if (numArgs != 2) {                                 \
    xstringb("incorrect number of arguments to " <<   \
             funcName << "; " << numArgs <<           \
             " passed but 2 required");               \
  }                                                   \
  string arg1 = toString(match.captured(2));          \
  string arg2 = toString(match.captured(3));


string EventReplay::runTest()
{
  // Let the application process any pending events so we reach a
  // state of quiescence.
  QCoreApplication::processEvents();

  QRegularExpression reBlank("^\\s*(#.*)?$");

  QRegularExpression reCall(
    "^\\s*(\\w+)"                      // 1: Function name
    "(?:\\s+\"([^\"]*)\")?"            // 2: Quoted optional arg 1
    "(?:\\s+\"([^\"]*)\")?"            // 3: Quoted optional arg 2
    "\\s*$");

  // Loop over all input lines.
  int lineNumber = 0;
  try {
    while (!m_in.eof()) {
      // Read the next line.
      lineNumber++;
      std::string line;
      getline(m_in, line);
      if (m_in.eof()) {
        break;
      }
      if (m_in.fail()) {
        throw string("error reading file");
      }
      QString qline(line.c_str());

      // Skip comments and blank lines.
      if (reBlank.match(qline).hasMatch()) {
        continue;
      }

      // Match call lines.
      QRegularExpressionMatch match(reCall.match(qline));
      if (match.hasMatch()) {
        QString funcName(match.captured(1));
        int numArgs = match.lastCapturedIndex() - 1;

        if (funcName == "KeyPress") {
          BIND_ARGS2(receiver, keys);

          QCoreApplication::postEvent(
            getWidgetFromPath(receiver),
            getKeyPressFromString(toString(match.captured(3))));
        }

        else if (funcName == "Check") {
          BIND_ARGS2(state, expect);

          string actual = m_query->eventReplayQuery(state);
          if (actual != expect) {
            xstringb(
              "Check \"" << state <<
              "\" should have been \"" << expect <<
              "\" but was \"" << actual << "\".");
          }
        }

        else if (funcName == "CheckLabel") {
          BIND_ARGS2(path, expect);

          QLabel *label = getQObjectFromPath<QLabel>(path);
          if (label->text() != toQString(expect)) {
            xstringb(
              "CheckLabel: \"" << path <<
              "\" should have been \"" << expect <<
              "\" but was \"" << label->text() << "\".");
          }
        }

        else {
          xstringb("unrecognized function: \"" << funcName << "\"");
        }
      }
      else {
        xstringb("unrecognized line: \"" << qline << "\"");
      }

      // After having injected an input event, let the application
      // settle back into quiescence.
      QCoreApplication::processEvents();
    } // while(!in.eof())
  }
  catch (string const &msg) {
    return stringb(m_fname << ':' << lineNumber << ": " << msg);
  }
  catch (xBase &x) {
    return stringb(m_fname << ':' << lineNumber << ": " << x.why());
  }

  return "";
}


// EOF
