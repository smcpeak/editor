// ui-shell-test-handler.cc
// Code for `ui-shell-test-handler` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-test-handler.h"               // this module

// smbase
#include "smbase/exc.h"                          // xmessage, xmessagesb
#include "smbase/gdvalue-parser.h"               // gdv::GDValueParser
#include "smbase/gdvalue-tuple.h"                // gdv::gdvpToTuple
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/sm-macros.h"                    // IMEMBFP
#include "smbase/stringb.h"                      // stringb
#include "smbase/xassert.h"                      // xassertPrecondition

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


SequencingTestHandler::SequencingTestHandler(
  std::vector<std::string> &log)
:
  UIShellCommandHandler(),
  IMEMBFP(log),
  m_nextStep(1),
  m_numSteps(0),
  m_throwAtStep(0)
{}


SequencingTestHandler::~SequencingTestHandler()
{}


GDValue SequencingTestHandler::executeDerivedCommand(
  std::string const &funcName,
  GDValueParser const &parser)
{
  if (funcName == "Mark") {
    auto [label] = gdvpToTuple<std::string>(parser);
    m_log.push_back("mark " + label);
    return GDValue();
  }

  m_log.push_back("command " + funcName);

  if (funcName == "Return") {
    auto [v] = gdvpToTuple<GDValue>(parser);
    return v;
  }

  else if (funcName == "Throw") {
    auto [msg] = gdvpToTuple<std::string>(parser);
    xmessage(msg);
  }

  else if (funcName == "Steps") {
    auto [n] = gdvpToTuple<int>(parser);
    m_nextStep = 1;
    m_numSteps = n;
    m_throwAtStep = 0;
  }

  else if (funcName == "StepThrows") {
    auto [n, k] = gdvpToTuple<int, int>(parser);
    m_nextStep = 1;
    m_numSteps = n;
    m_throwAtStep = k;
  }

  else {
    return UIShellCommandHandler::executeDerivedCommand(
      funcName, parser);
  }

  return GDValue();
}


UIShellQueryable *SequencingTestHandler::getQueryableFromPath(
  std::string const &path)
{
  xmessagesb("no queryables: " << path);
  return nullptr;    // not reached
}


bool SequencingTestHandler::hasPendingStep() const
{
  return m_nextStep <= m_numSteps;
}


void SequencingTestHandler::executePendingStep()
{
  xassertPrecondition(hasPendingStep());

  int step = m_nextStep++;
  m_log.push_back(stringb("step " << step));

  if (step == m_throwAtStep) {
    xmessagesb("step " << step << " failed");
  }
}


void SequencingTestHandler::discardPendingSteps()
{
  m_log.push_back("discard");
  m_nextStep = 1;
  m_numSteps = 0;
}


// EOF
