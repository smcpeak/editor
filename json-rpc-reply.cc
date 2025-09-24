// json-rpc-reply.cc
// Code for `json-rpc-reply` module.

#include "json-rpc-reply.h"            // this module

#include "smbase/compare-util.h"       // smbase::compare, RET_IF_COMPARE_MEMBERS
#include "smbase/gdvalue-either.h"     // gdv::toGDValue(smbase::Either)
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-parser.h"     // gdv::{GDValueParser, gdvpOptTo}

#include <iostream>                    // std::ostream

using namespace smbase;
using namespace gdv;


// -------------------------- JSON_RPC_Error ---------------------------
// ---- create-tuple-class: definitions for JSON_RPC_Error
/*AUTO_CTC*/ JSON_RPC_Error::JSON_RPC_Error(
/*AUTO_CTC*/   int code,
/*AUTO_CTC*/   std::string const &message,
/*AUTO_CTC*/   gdv::GDValue const &data)
/*AUTO_CTC*/   : IMEMBFP(code),
/*AUTO_CTC*/     IMEMBFP(message),
/*AUTO_CTC*/     IMEMBFP(data)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error::JSON_RPC_Error(
/*AUTO_CTC*/   int code,
/*AUTO_CTC*/   std::string &&message,
/*AUTO_CTC*/   gdv::GDValue &&data)
/*AUTO_CTC*/   : IMEMBMFP(code),
/*AUTO_CTC*/     IMEMBMFP(message),
/*AUTO_CTC*/     IMEMBMFP(data)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error::JSON_RPC_Error(JSON_RPC_Error const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_code),
/*AUTO_CTC*/     DMEMB(m_message),
/*AUTO_CTC*/     DMEMB(m_data)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error::JSON_RPC_Error(JSON_RPC_Error &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_code),
/*AUTO_CTC*/     MDMEMB(m_message),
/*AUTO_CTC*/     MDMEMB(m_data)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error &JSON_RPC_Error::operator=(JSON_RPC_Error const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_code);
/*AUTO_CTC*/     CMEMB(m_message);
/*AUTO_CTC*/     CMEMB(m_data);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error &JSON_RPC_Error::operator=(JSON_RPC_Error &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_code);
/*AUTO_CTC*/     MCMEMB(m_message);
/*AUTO_CTC*/     MCMEMB(m_data);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(JSON_RPC_Error const &a, JSON_RPC_Error const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   // Remember to #include "smbase/compare-util.h" for these.
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_code);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_message);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_data);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string JSON_RPC_Error::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, JSON_RPC_Error const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ JSON_RPC_Error::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "JSON_RPC_Error"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_code);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_message);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_data);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void JSON_RPC_Error::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


JSON_RPC_Error::JSON_RPC_Error()
  : m_code(),
    m_message(),
    m_data()
{}


void JSON_RPC_Error::setFromProtocol(gdv::GDValueParser const &p)
{
  GDVP_SET_MEMBER_STR(m_code);
  GDVP_SET_MEMBER_STR(m_message);
  GDVP_SET_OPT_MEMBER_STR(m_data);
}


/*static*/ JSON_RPC_Error JSON_RPC_Error::fromProtocol(
  gdv::GDValueParser const &p)
{
  JSON_RPC_Error obj;
  obj.setFromProtocol(p);
  return obj;
}


// -------------------------- JSON_RPC_Reply ---------------------------
void JSON_RPC_Reply::write(std::ostream &os) const
{
  toGDValue(*this).write(os,
    GDValueWriteOptions().setEnableIndentation(true));
}


// EOF
