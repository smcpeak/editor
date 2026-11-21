// file-mod-time.cc
// Code for `file-mod-time` module.

#include "file-mod-time.h"             // this module

// smbase
#include "smbase/nonport.h"            // getFileModificationTime

// libc++
#include <string>                      // std::string


FileTimestamp getFileModTime(std::string const &fname)
{
  FileTimestamp ret = 0;
  (void)getFileModificationTime(fname.c_str(), ret /*OUT*/);
  return ret;
}


// EOF
