#!/bin/sh
# Send out/sendlog/msg* to stdout.

# To use this script, run the editor with envvar
# `JSON_RPC_CLIENT_SEND_LOG_DIR` set to "out/sendlog" after making that
# directory (which, beware, "make clean" removes).  That will put a
# sequence of messages into files in that directory.
#
# Then, run this script from the `editor` directory and pipe its stdout
# to `clangd` to observe its behavior (on stderr).
#
# This is not part of any automated test, just a tool for low-level
# experimentation and debugging.

for fn in out/sendlog/msg*; do
  # Console output to indicate where we are.
  echo "" >&2
  echo "----- $fn -----" >&2

  # Send message contents to stdout (clangd).
  cat "$fn"

  sleep 2
done

# EOF
