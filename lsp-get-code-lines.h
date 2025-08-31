// lsp-get-code-lines.h
// `lspGetCodeLinesFunction` function.

// See license.txt for copyright and terms of use.

// Conceptually this module is just one method of `EditorGlobal`, but it
// is separated out to enable automated testing that is, in the current
// design, difficult to do with `EditorGlobal` directly.

#ifndef EDITOR_LSP_GET_CODE_LINES_H
#define EDITOR_LSP_GET_CODE_LINES_H

#include "host-file-and-line-opt-fwd.h"          // HostFileAndLineOpt [n]
#include "lsp-manager-fwd.h"                     // LSPManager [n]
#include "vfs-connections-fwd.h"                 // VFS_Connections [n]

#include "smqtutil/sync-wait-fwd.h"              // SynchronousWaiter [n]

#include "smbase/std-optional-fwd.h"             // std::optional [n]
#include "smbase/std-string-fwd.h"               // std::string [n]
#include "smbase/std-vector-fwd.h"               // stdfwd::vector [n]


// This is the core of `EditorGlobal::lspGetCodeLines`.  See its
// documentation for details.
std::optional<stdfwd::vector<std::string>> lspGetCodeLinesFunction(
  SynchronousWaiter &waiter,
  stdfwd::vector<HostFileAndLineOpt> const &locations,
  LSPManager &lspManager,
  VFS_Connections &vfsConnections);


#endif // EDITOR_LSP_GET_CODE_LINES_H
