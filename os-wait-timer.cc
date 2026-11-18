// os-wait-timer.cc
// Code for `os-wait-timer` module.

#include "os-wait-timer.h"             // this module

// smbase
#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/htt.h"                // smbase::HTT_Singleton
#include "smbase/sm-platform.h"        // PLATFORM_IS_WINDOWS
#include "smbase/sm-trace.h"           // INIT_TRACE, TRACE1
#include "smbase/xassert.h"            // xassert

// Qt
#include <QAbstractEventDispatcher>

// C++
#include <type_traits>                 // std::{is_same_v, remove_const_t}

using namespace smbase;


INIT_TRACE("os-wait-timer");


OSWaitTimer::OSWaitTimer()
  : QObject(),
    m_dispatcher(QAbstractEventDispatcher::instance()),
    m_labelIndex(HTT_Singleton::getIndexForLabel("OSWaitTimer")),
    m_active(PLATFORM_IS_WINDOWS != 0),
    m_node(nullptr)
{
  TRACE1("OSWaitTimer ctor");

  setObjectName("OSWaitTimer");

  // In this module's header file, I avoided including `htt.h` to
  // minimize #include weight, so now check that I declared
  // `m_labelIndex` with the right type.
  static_assert(std::is_same_v<
    std::remove_const_t<decltype(m_labelIndex)>,
    HTT_LabelIndex
  >);

  // If this object is created too early in the app lifecycle then there
  // might not be a dispatcher yet.
  xassert(m_dispatcher);

  if (m_active) {
    QObject::connect(m_dispatcher,
                     &QAbstractEventDispatcher::aboutToBlock,
                     this, &OSWaitTimer::slot_aboutToBlock);
    QObject::connect(m_dispatcher,
                     &QAbstractEventDispatcher::awake,
                     this, &OSWaitTimer::slot_awake);
  }
}


OSWaitTimer::~OSWaitTimer()
{
  TRACE1("OSWaitTimer dtor");

  // Check that the dispatcher hasn't changed.
  QAbstractEventDispatcher *curDispatcher =
    QAbstractEventDispatcher::instance();
  if (curDispatcher != m_dispatcher) {
    // This seems bad, so warn about it.  But we're in a dtor, so don't
    // throw an exception.
    DEV_WARNING("~OSWaitTimer: curDispatcher=" <<
                (void*)curDispatcher << ", m_dispatcher=" <<
                (void*)m_dispatcher);
  }

  if (m_active) {
    // Disconnect both signals.
    QObject::disconnect(m_dispatcher, NULL,
                        this, NULL);
  }

  if (m_node) {
    DEV_WARNING("~OSWaitTimer: Timer still running!");
  }
}


void OSWaitTimer::slot_aboutToBlock()
{
  TRACE1("OSWaitTimer slot_aboutToBlock");

  if (m_node) {
    DEV_WARNING("OSWaitTimer::slot_aboutToBlock: Timer already running!");
  }
  else {
    HTT_Tree &tree = HTT_Singleton::instanceTree();
    m_node = tree.getTOSChildNode(m_labelIndex);
    tree.pushNode(m_node);
  }
}


void OSWaitTimer::slot_awake()
{
  TRACE1("OSWaitTimer slot_awake");

  if (m_node) {
    HTT_Tree &tree = HTT_Singleton::instanceTree();
    tree.popNode(m_node);
    m_node = nullptr;
  }
  else {
    // Extra `awake` signals get sent (sort of unpredictably), so ignore
    // any that are not paired with a previous `aboutToBlock`.
  }
}


// EOF
