// recent-items-list.h
// `RecentItemsList`, a list of recently-used items.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RECENT_ITEMS_LIST_H
#define EDITOR_RECENT_ITEMS_LIST_H

#include "recent-items-list-iface.h"   // interface for this module

#include "smbase/xassert.h"            // xassertPrecondition

#include <algorithm>                   // std::find
#include <list>                        // std::list


template <typename T>
RecentItemsList<T>::~RecentItemsList()
{}


template <typename T>
RecentItemsList<T>::RecentItemsList()
  : m_list()
{}


template <typename T>
void RecentItemsList<T>::selfCheck() const
{
  // This uses an inefficient nested loop because I do not want to
  // require that `T` have a relational comparison operator, and because
  // I do not expect this list to be long.
  for (auto it = m_list.begin(); it != m_list.end(); ++it) {
    auto it2 = it;
    ++it2;
    for (; it2 != m_list.end(); ++it2) {
      xassert(*it != *it2);
    }
  }
}


template <typename T>
bool RecentItemsList<T>::empty() const
{
  return m_list.empty();
}


template <typename T>
void RecentItemsList<T>::clear()
{
  return m_list.clear();
}


template <typename T>
std::list<T> const &RecentItemsList<T>::getListC() const
{
  return m_list;
}


template <typename T>
void RecentItemsList<T>::add(T const &t)
{
  auto it = std::find(m_list.begin(), m_list.end(), t);
  if (it != m_list.end()) {
    // Move the existing item to the front.
    m_list.splice(m_list.begin(), m_list, it);
  }
  else {
    // Prepend it.
    m_list.push_front(t);
  }
}


template <typename T>
void RecentItemsList<T>::remove(T const &t)
{
  auto it = std::find(m_list.begin(), m_list.end(), t);
  if (it != m_list.end()) {
    m_list.erase(it);
  }
}


template <typename T>
T const &RecentItemsList<T>::getRecentOther(T const &t)
{
  for (T const &other : m_list) {
    if (other != t) {
      return other;
    }
  }

  // No alternative.
  return t;
}


#endif // EDITOR_RECENT_ITEMS_LIST_H
