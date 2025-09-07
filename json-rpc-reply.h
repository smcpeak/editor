// json-rpc-reply.h
// `JSON_RPC_Reply` class, carrying the reply for a JSON-RPC request.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_JSON_RPC_REPLY_H
#define EDITOR_JSON_RPC_REPLY_H

#include "json-rpc-reply-fwd.h"        // fwds for this module

#include "smbase/either.h"             // smbase::Either
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]
#include "smbase/gdvalue.h"            // gdv::GDValue

#include <iosfwd>                      // std::ostream [n]
#include <string>                      // std::string


// Error object from some reply.
class JSON_RPC_Error {
public:      // data
  // Numeric error code.
  int m_code;

  // Human-readable description.  The spec says this should be "a
  // concise single sentence".
  std::string m_message;

  // Additional data, if any.
  gdv::GDValue m_data;

public:      // methods
  // ---- create-tuple-class: declarations for JSON_RPC_Error +move +write +gdvWrite
  /*AUTO_CTC*/ explicit JSON_RPC_Error(int code, std::string const &message, gdv::GDValue const &data);
  /*AUTO_CTC*/ explicit JSON_RPC_Error(int code, std::string &&message, gdv::GDValue &&data);
  /*AUTO_CTC*/ JSON_RPC_Error(JSON_RPC_Error const &obj) noexcept;
  /*AUTO_CTC*/ JSON_RPC_Error(JSON_RPC_Error &&obj) noexcept;
  /*AUTO_CTC*/ JSON_RPC_Error &operator=(JSON_RPC_Error const &obj) noexcept;
  /*AUTO_CTC*/ JSON_RPC_Error &operator=(JSON_RPC_Error &&obj) noexcept;
  /*AUTO_CTC*/ // For +write:
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, JSON_RPC_Error const &obj);
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;

  JSON_RPC_Error();

  // create-tuple-class "+gdvRead" would make a method that parses what
  // `operator GDValue()` produces, but that is different from how these
  // are represented in the protocol.
  void setFromProtocol(gdv::GDValueParser const &p);
  static JSON_RPC_Error fromProtocol(gdv::GDValueParser const &p);
};


// A reply to a request is either an error, or the "result" of a
// successful response.
class JSON_RPC_Reply :
                   public smbase::Either<gdv::GDValue, JSON_RPC_Error> {
public:      // types
  using Base = smbase::Either<gdv::GDValue, JSON_RPC_Error>;

public:      // methods
  // Inherit ctors.
  //
  // Note: There is an implicit conversion from `JSON_RPC_Error` to
  // `GDValue`, so a little care is required to not accidentally
  // construct the wrong thing.
  using Base::Base;

  // True if this reply indicates the request was successfully executed.
  bool isSuccess() const
    { return isLeft(); }

  // Get the "result" portion of the reply.
  //
  // Requires: isSuccess()
  gdv::GDValue const &result() const
    { return left(); }
  gdv::GDValue &result()
    { return left(); }

  // True if this reply indicates the request encountered an error.
  bool isError() const
    { return isRight(); }

  // Get the "error" portion of the reply.
  //
  // Requires: isError()
  JSON_RPC_Error const &error() const
    { return right(); }
  JSON_RPC_Error &error()
    { return right(); }

  // Write as GDV indented string.
  void write(std::ostream &os) const;
  friend std::ostream &operator<<(std::ostream &os,
                                  JSON_RPC_Reply const &obj)
    { obj.write(os); return os; }
};


#endif // EDITOR_JSON_RPC_REPLY_H
