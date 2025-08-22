#!/usr/bin/env python3
"""
Script that acts as a Language Server Protocol server for the purpose of
testing an LSP client.  It checks that the client follows the correct
procedure for startup and shutdown, exiting nonzero if there is a
problem.

It responds to the "textDocument/didOpen" notification with a
"textDocument/publishDiagnostics" notification.

It also tracks changes sent via "textDocument/didChange", maintaining an
internal copy of the contents, which can then be retrieved with
"$/getTextDocumentContents".

It accepts and checks for well-formedness all other notifications, but
does not respond to them.  It replies to all requests other than startup
and shutdown with an empty result.
"""

import json
import os
import sys
import traceback

from typing import Any, Dict, List, Tuple


# Positive if debug is enabled, with higher values enabling more printing.
debugLevel: int = 0
if debugEnvVal := os.getenv("DEBUG"):
  debugLevel = int(debugEnvVal)


# Use binary mode for stdin/stdout for LSP protocol.
stdin = sys.stdin.buffer
stdout = sys.stdout.buffer

# State flags to validate the correct sequence.
initialized = False
shutdown_received = False

# Map from document URI to its current (version, contents).
documents: Dict[str, Tuple[int, str]] = {}


def read_message() -> Dict[str, Any]:
  """
  Read one LSP message from stdin.
  """
  headers: Dict[str, str] = {}
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
  result = json.loads(body)
  if not isinstance(result, dict):
    raise RuntimeError("JSON payload is not a dictionary")

  return result


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


def expect_eq(actual: Any, expect: Any) -> None:
  """
  Complain if `actual` is not `expect`.
  """
  if actual != expect:
    complain("expect_eq: values are not equal:\n"+
      f"  actual: {actual!r}\n"
      f"  expect: {expect!r}")


def split_lines(text: str) -> List[str]:
  """
  Divide `text` into lines, treating the newline character as a
  separator.  The newlines are retained for all but the last element.
  The concatenation of the returned list equals the original string.
  """

  parts = text.split("\n")

  # Reattach '\n' to every part except the last.
  lines = [p + "\n" for p in parts[:-1]]

  lines.append(parts[-1])
  return lines


def test_split_lines() -> None:
  """Unit tests for `split_lines`."""

  expect_eq(split_lines(""), [""])
  expect_eq(split_lines("\n"), ["\n", ""])
  expect_eq(split_lines("a\nb"), ["a\n", "b"])
  expect_eq(split_lines("a\nb\n"), ["a\n", "b\n", ""])
  expect_eq(split_lines("a\n\nb\n"), ["a\n", "\n", "b\n", ""])


def apply_text_edits(old_text: str, changes: List[Dict[str, Any]]) -> str:
  """
  Apply a list of LSP TextDocumentContentChangeEvent edits to the old text.
  - If "range" is missing, it's a full replacement.
  - Otherwise, apply range-based incremental edits.
  """
  text = old_text
  for change in changes:
    if "range" not in change or change["range"] is None:
      # Full text replacement
      text = change["text"]
    else:
      rng = change["range"]
      start = rng["start"]
      end = rng["end"]

      def offset(pos: Dict[str, int]) -> int:
        """Convert line/character to byte offset into text."""

        lines = split_lines(text)

        line_idx = pos["line"]
        char_idx = pos["character"]

        if not 0 <= line_idx < len(lines):
          raise RuntimeError("Position line out of range")

        if not 0 <= char_idx <= len(lines[line_idx]):
          raise RuntimeError("Position character out of range")

        line_start = sum(len(lines[i]) for i in range(line_idx))
        return line_start + char_idx

      start_off = offset(start)
      end_off = offset(end)
      text = text[:start_off] + change["text"] + text[end_off:]

  return text


def test_one_apply_text_edit(
  start_line: int,
  start_character: int,
  end_line: int,
  end_character: int,
  edit_text: str,
  orig_text: str,
  expect: str) -> str:
  """
  Apply a single edit to `orig_text`, expecting `expect`, which is
  returned.
  """

  changes = [
    {
      "range": {
        "start": {
          "line": start_line,
          "character": start_character,
        },
        "end": {
          "line": end_line,
          "character": end_character,
        },
      },
      "text": edit_text,
    }
  ]

  actual: str = apply_text_edits(orig_text, changes)
  expect_eq(actual, expect);

  return actual


def test_apply_text_edits() -> None:
  """Unit tests for `apply_text_edits`."""

  text = ""
  text = test_one_apply_text_edit(0,0,0,0, "zero\none\ntwo\n",
    text, "zero\none\ntwo\n")
  text = test_one_apply_text_edit(3,0,3,0, "three\nfour",
    text, "zero\none\ntwo\nthree\nfour")
  text = test_one_apply_text_edit(1,1,2,2, "ZZZ",
    text, "zero\noZZZo\nthree\nfour")
  text = test_one_apply_text_edit(0,4,0,4, "ZERO",
    text, "zeroZERO\noZZZo\nthree\nfour")
  text = test_one_apply_text_edit(0,0,0,0, "ABC",
    text, "ABCzeroZERO\noZZZo\nthree\nfour")


def publish_diagnostics(uri: str, version: int) -> None:
  """Send empty diagnostics notification."""
  publish = {
    "jsonrpc": "2.0",
    "method": "textDocument/publishDiagnostics",
    "params": {
      "uri": uri,
      "diagnostics": [],

      # My `lsp-manager` module will discard diagnostics that do
      # not have a version number.
      "version": version,
    },
  }
  send_message(publish)


def main() -> None:
  global initialized, shutdown_received

  if os.getenv("UNIT_TEST"):
    test_split_lines()
    test_apply_text_edits()
    return

  try:
    while True:
      msg = read_message()
      if "method" not in msg:
        complain("Message is missing `method` attribute.")

      method = msg["method"]

      # Requests have an ID; notifications do not.
      msg_id = msg.get("id")

      if method == "initialize":
        # Reply with capabilities
        result = {
          "capabilities": {
            "textDocumentSync": {
              # This is what `clangd` sends for this key.
              "change": 2,
              "openClose": True,
              "save": True,
            }
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
        # Save the contents and respond with a publishDiagnostics
        # notification.
        td = msg["params"]["textDocument"]
        uri = td["uri"]
        version = td["version"]
        text = td["text"]
        documents[uri] = (version, text)
        publish_diagnostics(uri, version)

      elif method == "textDocument/didChange":
        # Record the incremental edits, then send diagnostics.
        td = msg["params"]["textDocument"]
        uri = td["uri"]
        version = td["version"]
        changes = msg["params"]["contentChanges"]
        old_text = documents.get(uri, "")[1]
        documents[uri] = (version, apply_text_edits(old_text, changes))
        publish_diagnostics(uri, version)

      elif method == "$/getTextDocumentContents":
        td = msg["params"]["textDocument"]
        uri = td["uri"]
        version, contents = documents.get(uri, (-1, "<no doc>"))
        send_message({
          "jsonrpc": "2.0",
          "id": msg_id,
          "result": {
            "uri": uri,
            "text": contents,
            "version": version
          }
        })

      else:
        # For other requests, respond with empty result
        if msg_id is not None:
          send_message({"jsonrpc": "2.0", "id": msg_id, "result": None})
        # Notifications: just accept and ignore

  except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    if debugLevel >= 1:
      traceback.print_exc(file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
  main()


# EOF
