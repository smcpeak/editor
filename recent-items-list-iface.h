// recent-items-list-iface.h
// Interface for `recent-items-list`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RECENT_ITEMS_LIST_IFACE_H
#define EDITOR_RECENT_ITEMS_LIST_IFACE_H

#include "recent-items-list-fwd.h"     // fwds for this module

#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES

#include <list>                        // std::list
#include <optional>                    // std::optional


/* The type `T` must allow:

   * Copying into the list.

   * Equality comparison for search and insertion.
*/
template <typename T>
class RecentItemsList {
  // For now.
  NO_OBJECT_COPIES(RecentItemsList);

private:     // data
  // List of items, with most-recently-used at the front.
  //
  // Invariant: No element appears more than once.
  std::list<T> m_list;

public:      // methods
  inline ~RecentItemsList();

  // Initially empty list.
  //
  // Ensures: empty()
  inline RecentItemsList();

  // Assert invariants.
  inline void selfCheck() const;

  // True if there are no elements.
  inline bool empty() const;

  // Remove all elements.
  //
  // Ensures: empty()
  inline void clear();

  // Read-only list access.
  inline std::list<T> const &getListC() const;

  // Add or move `t` to the front.
  //
  // Ensures: !empty()
  inline void add(T const &t);

  // Remove `t` if present.
  inline void remove(T const &t);

  // Get the most recent item other than `t`.  If there isn't anything
  // other than `t`, return `t`.
  inline T const &getRecentOther(T const &t);

  // Get the first, i.e. most recent, element, if there is one.
  inline std::optional<T> firstOpt() const;
};


#endif // EDITOR_RECENT_ITEMS_LIST_IFACE_H
