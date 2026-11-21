// reply-ptr-opt.h
// `ReplyPtrOpt` and `ReplyPtrOptOrError` class templates.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_REPLY_PTR_OPT_H
#define EDITOR_REPLY_PTR_OPT_H

#include "reply-ptr-opt-fwd.h"         // fwds for this module

// editor
#include "fail-reason-opt.h"           // FailReasonOpt
#include "rfa-msg.h"                   // RFA_Message

// smbase
#include "smbase/either.h"             // smbase::Either

// libc++
#include <memory>                      // std::unique_ptr
#include <string>                      // std::string
#include <type_traits>                 // std::is_base_of_v
#include <utility>                     // std::move


/*
  Hold a REPLY_TYPE, or nullptr to indicate the request was canceled or
  that it yielded an error but the error has already been handled.

  There are three cases:

    1. Success reply.

    2. Error reply: error on the server, for example a file does not
       exist.

    3. Null pointer: user canceled the wait, or there was an error but
       it has been handled.

  Structurally, this is identical to `std::unique_ptr<REPLY_TYPE>`, but
  I use a distinct class name to convey the particular semantics related
  to IPC replies, especially the existence and meaning of nullptr.

  TODO: Replace `std::unique_ptr` with my `smbase::UniquePtr`.
*/
template <typename REPLY_TYPE>
class ReplyPtrOpt : public std::unique_ptr<REPLY_TYPE> {
  // Ensure this class is only used for its intended purpose.
  static_assert(std::is_base_of_v<RFA_Message, REPLY_TYPE>);

public:      // types
  using Base = std::unique_ptr<REPLY_TYPE>;

public:      // methods
  ~ReplyPtrOpt()
    = default;

  // Create an absent value.
  ReplyPtrOpt()
    : Base() {}

  // Create a present or absent value depending on `ptr`.  If `ptr` is
  // not nullptr, this takes ownership of it.
  ReplyPtrOpt(REPLY_TYPE * NULLABLE ptr)
    : Base(ptr) {}

  ReplyPtrOpt(ReplyPtrOpt &&obj)
    : Base(std::move(obj)) {}

  ReplyPtrOpt &operator=(ReplyPtrOpt &&obj)
    { Base::operator=(std::move(obj)); return *this; }
  ReplyPtrOpt &operator=(Base &&obj)
    { Base::operator=(std::move(obj)); return *this; }

  bool hasValue() const
    { return Base::operator bool(); }
  bool wasCanceled() const
    { return !hasValue(); }
};


/*
  Hold a ReplyPtrOpt (reply or cancelation) as "left", or an error
  message (that has *not* been handled) as "right".

  There are four cases:

    1. Success reply in left alternative.

    2. Error reply in left alternative: error on the server, for example
       a file does not exist.

    3. Null pointer in left alternative: user canceled the wait, or an
       error occurred but has already been handled.

    4. Error message in right alternative: RFA communication error, for
       example the connection was terminated.

  The `getEitherErrorMessage` method can be used to combine the handling
  of the error cases (2 and 4).
*/
template <typename REPLY_TYPE>
class ReplyPtrOptOrError :
           public smbase::Either<ReplyPtrOpt<REPLY_TYPE>, std::string> {
public:      // types
  using Base = smbase::Either<ReplyPtrOpt<REPLY_TYPE>, std::string>;

public:      // methods
  ~ReplyPtrOptOrError()
    = default;

  // Create a cancelation (absent reply) value.
  ReplyPtrOptOrError()
    : Base() {}

  // Create a present or absent reply value depending on `obj`.
  ReplyPtrOptOrError(ReplyPtrOpt<REPLY_TYPE> &&reply)
    : Base(std::move(reply)) {}

  // Create a comm error (right) value.
  ReplyPtrOptOrError(std::string const &msg)
    : Base(msg) {}

  ReplyPtrOptOrError(ReplyPtrOptOrError &&obj)
    : Base(std::move(obj)) {}

  /* If `replyOrError` has an error message in either half, extract and
     return it.

     If this returns nullptr, then the caller can be sure this object
     carries a reply object that is either null or successful.

     That is, this function ensures:

       (return == nullopt) ==>
         (isLeft() &&
          (left() != nullptr) ==>
            left()->m_success
         ))
  */
  FailReasonOpt getEitherErrorMessage() const
  {
    if (this->isRight()) {
      return this->right();
    }
    else {
      std::unique_ptr<REPLY_TYPE> const &reply = this->left();
      if (reply && !reply->m_success) {
        return stringb(
          reply->m_failureReasonString <<
          " (code " << reply->m_failureReasonCode << ")");
      }
    }

    return std::nullopt;
  }
};


#endif // EDITOR_REPLY_PTR_OPT_H
