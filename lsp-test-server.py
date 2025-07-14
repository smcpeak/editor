#!/usr/bin/env python3
"""
Script that acts as a Language Server Protocol server for the purpose of
testing an LSP client.  It checks that the client follows the correct
procedure for startup and shutdown, exiting nonzero if there is a
problem.  It responds to the "textDocument/didOpen" notification with a
"textDocument/publishDiagnostics" notification.  It accepts and checks
for well-formedness all other notifications, but does not respond to
them.  It replies to all requests other than startup and shutdown with
an empty result.
"""

# This script was written largely by ChatGPT, based on a prompt very
# similar to the description above.

import sys
import json
import struct

from typing import Any, Dict

import io

# Use binary mode for stdin/stdout so we can control encoding and buffering
stdin = sys.stdin.buffer
stdout = sys.stdout.buffer

# State flags to validate the correct sequence
initialized = False
shutdown_received = False


def read_message() -> Dict[str, Any]:
  """
  Read one LSP message from stdin.
  """
  headers = {}
  while True:
    line = stdin.readline().decode("utf-8").strip()
    if line == "":
      break
    key, value = line.split(":", 1)
    headers[key.strip()] = value.strip()

  content_length = int(headers.get("Content-Length", 0))
  if content_length == 0:
    raise RuntimeError("Missing Content-Length")

  body = stdin.read(content_length).decode("utf-8")
  return json.loads(body)


def send_message(message: Dict[str, Any]) -> None:
  """
  Send one LSP message to stdout.
  """
  body = json.dumps(message, separators=(",", ":"))
  body_bytes = body.encode("utf-8")
  header = f"Content-Length: {len(body_bytes)}\r\n\r\n".encode("utf-8")
  stdout.write(header + body_bytes)
  stdout.flush()


def complain(msg: str) -> None:
  """Print `msg` to stderr and quit."""

  print(msg, file=sys.stderr);
  sys.exit(2);


def main():
  global initialized, shutdown_received

  try:
    while True:
      msg = read_message()
      if "method" in msg:
        method = msg["method"]

        # Requests have an ID; notifications do not.
        msg_id = msg.get("id")

        if method == "initialize":
          # Reply with capabilities
          result = {
            "capabilities": {
              "textDocumentSync": 1
            }
          }
          send_message({"jsonrpc": "2.0", "id": msg_id, "result": result})

        elif method == "initialized":
          initialized = True

        elif method == "shutdown":
          if not initialized:
            complain("Received `shutdown` without `initialized`.")
          shutdown_received = True
          send_message({"jsonrpc": "2.0", "id": msg_id, "result": None})

        elif method == "exit":
          if not shutdown_received:
            complain("Received `exit` without `shutdown`.")
          sys.exit(0)

        elif method == "textDocument/didOpen":
          # Respond with a publishDiagnostics notification
          uri = msg["params"]["textDocument"]["uri"]
          diagnostics = []
          publish = {
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
              "uri": uri,
              "diagnostics": diagnostics
            }
          }
          send_message(publish)

        else:
          # For other requests, respond with empty result
          if msg_id is not None:
            send_message({"jsonrpc": "2.0", "id": msg_id, "result": None})
          # Notifications: just accept and ignore

      else:
        complain("Message is missing `method` attribute.")

  except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
  main()


# EOF
