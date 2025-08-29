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
does not respond to them.  It replies to all unknown requests with an
empty result.
"""

import json
import os
import re
import sys
import time                  # sleep
import traceback

from enum import Enum
from functools import total_ordering
from typing import Any, Dict, List, Optional, Tuple


# ------------------------------ Globals -------------------------------
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


# -------------------------- General purpose ---------------------------
def complain(msg: str) -> None:
  """Print `msg` to stderr and quit."""

  print(msg, file=sys.stderr);
  sys.exit(2);


def compare_structured_values(val1: Any, val2: Any, path: str = "") -> None:
  """
  Compares two structured values (dictionaries, lists, or primitive types)
  and prints where they differ.

  Args:
    val1: The first value to compare.
    val2: The second value to compare.
    path: The current path in the structure (for reporting differences).
  """

  if type(val1) != type(val2):
    print(f"Type mismatch at '{path}': '{type(val1).__name__}' vs '{type(val2).__name__}'")
    return

  if isinstance(val1, dict):
    # Compare dictionaries
    all_keys = set(val1.keys()) | set(val2.keys())
    for key in sorted(all_keys):
      new_path = f"{path}.{key}" if path else key
      if key not in val1:
        print(f"Key '{new_path}' missing in first value.")
      elif key not in val2:
        print(f"Key '{new_path}' missing in second value.")
      else:
        compare_structured_values(val1[key], val2[key], new_path)
  elif isinstance(val1, list):
    # Compare lists
    if len(val1) != len(val2):
      print(f"List length mismatch at '{path}': {len(val1)} vs {len(val2)}")

    for i in range(min(len(val1), len(val2))):
      new_path = f"{path}[{i}]"
      compare_structured_values(val1[i], val2[i], new_path)
  else:
    # Compare primitive types
    if val1 != val2:
      print(f"Value mismatch at '{path}':\n"+
            f"  actual: {val1!r}\n"+
            f"  expect: {val2!r}")


def expect_eq(actual: Any, expect: Any) -> None:
  """
  Complain if `actual` is not `expect`.
  """
  if actual != expect:
    compare_structured_values(actual, expect)
    complain("expect_eq: values are not equal:\n"+
      f"  actual: {actual!r}\n"
      f"  expect: {expect!r}")


def compare(a: Any, b: Any) -> int:
  """
  Return -1 if a < b, 0 if a == b, 1 if a > b.
  """
  if a < b:
    return -1
  elif a > b:
    return 1
  else:
    return 0


def check_compare(a: Any, b: Any, expect: int) -> None:
  """Check that comparing `a` and `b` yields `expect`."""

  expect_eq(compare(a, b), expect);

  # Verify all the binary comparison operators.
  expect_eq(a == b, expect == 0);
  expect_eq(a != b, expect != 0);
  expect_eq(a <= b, expect <= 0);
  expect_eq(a <  b, expect <  0);
  expect_eq(a >= b, expect >= 0);
  expect_eq(a >  b, expect >  0);


def check_compare_sym(a: Any, b: Any, expect: int) -> None:
  """Check that comparing `a` and `b` yields `expect`, symmetrically."""

  check_compare(a, b, expect)
  check_compare(b, a, -expect)


def check_sorted(lst: List[Any]) -> None:
  """Check that `lst` is sorted."""

  i = 0
  while i < len(lst):
    a = lst[i]
    check_compare(a, a, 0)

    j = i+1
    while j < len(lst):
      b = lst[j]
      check_compare_sym(a, b, -1)
      j += 1

    i += 1


# ------------------------------ Position ------------------------------
@total_ordering
class Position:
  """Position within a document."""

  def __init__(self, line: int, character: int) -> None:
    # 0-based line number.
    self.line = int(line)

    # 0-based character number within a line.
    self.character = int(character)

  def to_dict(self) -> dict[str, int]:
    """Return a dict suitable for use in LSP."""
    return {"line": self.line, "character": self.character}

  @classmethod
  def from_dict(cls, data: dict[str, int]) -> "Position":
    """Construct a position from LSP data."""
    return cls(data["line"], data["character"])

  def __repr__(self) -> str:
    return f"Position(line={self.line}, character={self.character})"

  def compare(self, other: "Position") -> int:
    if not isinstance(other, Position):
      raise TypeError(f"Cannot compare Position with {type(other)}")

    c = compare(self.line, other.line)
    if c != 0:
      return c;

    return compare(self.character, other.character)

  def __eq__(self, other: Any) -> bool:
    if not isinstance(other, Position):
      return NotImplemented
    return self.compare(other) == 0

  def __lt__(self, other: Any) -> bool:
    if not isinstance(other, Position):
      return NotImplemented
    return self.compare(other) < 0


def test_Position() -> None:
  """Unit tests for `Position`."""

  p1 = Position(1, 1)
  p2 = Position(1, 5)
  p3 = Position(2, 2)

  d = p2.to_dict()
  expect_eq(d, { "line": 1, "character": 5})
  expect_eq(Position.from_dict(d), p2)

  check_sorted([p1, p2, p3])

  # Check that equality is structural.
  p1b = Position(1, 1)
  expect_eq(p1 == p1b, True)
  expect_eq(p1 != p1b, False)


# ------------------------------- Range --------------------------------
@total_ordering
class Range:
  """Range of positions within a document."""

  def __init__(self, start: Position, end: Position) -> None:
    # Inclusive start point.
    self.start = start

    # Exclusive end point.
    self.end = end

    assert(self.start <= self.end)

  def to_dict(self) -> dict[str, dict[str, int]]:
    """Return a dict suitable for use in LSP."""
    return {"start": self.start.to_dict(),
            "end":   self.end.to_dict()}

  @classmethod
  def from_dict(cls, data: dict[str, dict[str, int]]) -> "Range":
    """Construct a range from LSP data."""
    return cls(Position.from_dict(data["start"]),
               Position.from_dict(data["end"]))

  def __repr__(self) -> str:
    return f"Range(start={self.start}, end={self.end})"

  def compare(self, other: "Range") -> int:
    if not isinstance(other, Range):
      raise TypeError(f"Cannot compare Range with {type(other)}")

    c = compare(self.start, other.start)
    if c != 0:
      return c;

    return compare(self.end, other.end)

  def __eq__(self, other: Any) -> bool:
    if not isinstance(other, Range):
      return NotImplemented
    return self.compare(other) == 0

  def __lt__(self, other: Any) -> bool:
    if not isinstance(other, Range):
      return NotImplemented
    return self.compare(other) < 0

  def contains(self, pos: Position) -> bool:
    return self.start <= pos < self.end


def test_Range() -> None:
  """Unit tests for `Range`."""

  p1 = Position(1, 1)
  p2 = Position(1, 5)
  p3 = Position(2, 2)

  r11 = Range(p1, p1)
  r12 = Range(p1, p2)
  r13 = Range(p1, p3)
  r22 = Range(p2, p2)
  r23 = Range(p2, p3)
  r33 = Range(p3, p3)

  d = r12.to_dict()
  expect_eq(d, {
    "start": { "line": 1, "character": 1 },
    "end":   { "line": 1, "character": 5 },
  })
  expect_eq(Range.from_dict(d), r12)

  check_sorted([r11, r12, r13, r22, r23, r33])

  expect_eq(r11.contains(p1), False)
  expect_eq(r12.contains(p1), True)
  expect_eq(r13.contains(p2), True)
  expect_eq(r13.contains(p3), False)


# ---------------------------- split_lines -----------------------------
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


# ----------------------------- get_words ------------------------------
word_re = re.compile(r"[a-zA-Z_][a-zA-Z0-9_]*")

def get_words(text: str) -> List[Tuple[int, str]]:
  """
  Partition `text` into words matching the regex [a-zA-Z_][a-zA-Z0-9_]*.

  Return them as a list of `(offset, word)` tuples, where `offset` is
  the character index in `str` of the first letter of `word`.
  """
  return [(m.start(), m.group()) for m in word_re.finditer(text)]


def test_get_words() -> None:
  """Unit tests for `get_words`."""

  expect_eq(get_words(""), [])
  expect_eq(get_words("hello"), [(0, "hello")])
  expect_eq(get_words("foo bar baz"),
            [(0, "foo"), (4, "bar"), (8, "baz")])
  expect_eq(get_words("foo_bar x9 _start end2"),
            [(0, "foo_bar"), (8, "x9"), (11, "_start"), (18, "end2")])
  expect_eq(get_words("42not valid123 valid_word"),
            [(2, "not"), (6, "valid123"), (15, "valid_word")])
  expect_eq(get_words("alpha,beta.gamma;delta"),
            [(0, "alpha"), (6, "beta"), (11, "gamma"), (17, "delta")])

  # Only ASCII letters, underscore, and digits should match
  expect_eq(get_words("café naïve jalapeño"),
            [(0, "caf"), (5, "na"), (8, "ve"), (11, "jalape"), (18, "o")])

  # Digits alone should not match
  expect_eq(get_words("123 456"), [])


# ---------------------------- SymbolTable -----------------------------
declMethod = "textDocument/declaration"
defnMethod = "textDocument/definition"
refsMethod = "textDocument/references"

class OccurrenceKind(Enum):
  DECLARATION = 1
  DEFINITION = 2
  REFERENCE = 3

  def satisfies(self, method: str) -> bool:
    """True if this kind is responsive for `method`."""

    if method == declMethod:
      return self == OccurrenceKind.DECLARATION

    elif method == defnMethod:
      return self == OccurrenceKind.DEFINITION

    elif method == refsMethod:
      return True

    else:
      return False


class Occurrence:
  """Occurrence of a symbol within a document."""

  def __init__(self, name: str, kind: OccurrenceKind, range: Range) -> None:
    self.name = name
    self.kind = kind
    self.range = range

  def __repr__(self) -> str:
    return f"Occurrence(name={self.name!r}, kind={self.kind}, range={self.range})"

  def __eq__(self, other: Any) -> bool:
    if not isinstance(other, Occurrence):
      return NotImplemented
    return (self.name == other.name and
            self.kind == other.kind and
            self.range == other.range)


# Collection of all Occurrences for a document.
SymbolTable = dict[str, List[Occurrence]]


def brace_on_line_or_next(lines: List[str], line_index: int) -> bool:
  """True if there is a "{" character on `lines[line_index]` or
  `lines[line_index+1]`."""

  return ("{" in lines[line_index] or
          (line_index+1 < len(lines) and
           "{" in lines[line_index+1]))


def test_brace_on_line_or_next() -> None:
  """Unit tests for `brace_on_line_or_next`."""

  assert(brace_on_line_or_next(["x"], 0) == False)
  assert(brace_on_line_or_next(["{"], 0) == True)
  assert(brace_on_line_or_next(["x{"], 0) == True)
  assert(brace_on_line_or_next(["x", "{"], 0) == True)
  assert(brace_on_line_or_next(["x", "{"], 1) == True)
  assert(brace_on_line_or_next(["x", "y", "{"], 0) == False)


def build_symbol_table(contents: str) -> SymbolTable:
  """
  Scan `contents` to build a symbol table.

  Every word in `contents` matching the regex [a-zA-Z_][a-zA-Z0-9_]*
  is considered to be an occurrence.

  If there is a "{" on the same line or on the next line, then the
  occurrence is a DEFINITION.  Otherwise, if it is the first occurrence
  in `contents`, then it is a DECLARATION, otherwise a REFERENCE.
  """

  table: SymbolTable = {}

  lines: List[str] = split_lines(contents)

  line_index: int = 0
  for line in lines:
    for character_index, word in get_words(line):
      occurrences = table.setdefault(word, [])

      kind = OccurrenceKind.REFERENCE
      if brace_on_line_or_next(lines, line_index):
        kind = OccurrenceKind.DEFINITION
      elif len(occurrences) == 0:
        kind = OccurrenceKind.DECLARATION

      occurrences.append(Occurrence(
        word,
        kind,
        Range(
          Position(line_index, character_index),
          Position(line_index, character_index + len(word))
        )))

    line_index += 1

  return table


def sym_find_name_by_position(
  table: SymbolTable,
  pos: Position) -> Optional[str]:
  """Return the name of the symbol at the occurrence enclosing `pos`
  if there is one."""

  for name, occurrences in table.items():
    for occurrence in occurrences:
      if occurrence.range.contains(pos):
        return name

  return None


sample_document: str = (
"""one two three
one four

one{}

two
{}
three""")


def test_build_symbol_table() -> None:
  """Unit tests for `build_symbol_table`."""

  table: SymbolTable = build_symbol_table(sample_document)

  decl = OccurrenceKind.DECLARATION
  defn = OccurrenceKind.DEFINITION
  ref = OccurrenceKind.REFERENCE

  def r(a: int, b: int, c: int, d: int) -> Range:
    return Range(Position(a,b), Position(c,d))

  expect_eq(table, {
    "one": [
      Occurrence("one", decl, r(0, 0, 0, 3)),
      Occurrence("one", ref,  r(1, 0, 1, 3)),
      Occurrence("one", defn, r(3, 0, 3, 3)),
    ],
    "two": [
      Occurrence("two", decl, r(0, 4, 0, 7)),
      Occurrence("two", defn, r(5, 0, 5, 3)),
    ],
    "three": [
      Occurrence("three", decl, r(0, 8, 0, 13)),
      Occurrence("three", ref,  r(7, 0, 7, 5)),
    ],
    "four": [
      Occurrence("four", decl, r(1, 4, 1, 8)),
    ],
  })

  expect_eq(sym_find_name_by_position(table, Position(0,0)), "one")
  expect_eq(sym_find_name_by_position(table, Position(0,3)), None)
  expect_eq(sym_find_name_by_position(table, Position(1,5)), "four")


# ------------------------ handle_symbol_query -------------------------
def get_symbols(
  method: str,
  table: SymbolTable,
  pos: Position) -> List[Occurrence]:
  """Get the occurrences of the symbol at `pos` satisfying `method`."""

  # Occurrences that satisfy the method.
  satisfying_occurrences: List[Occurrence] = []

  # Locate the symbol under `pos` in the table.
  name: Optional[str] = sym_find_name_by_position(table, pos)
  if name is not None:
    occurrences = table[name]

    # Collect the occurrences satisfying `method`.
    for occurrence in occurrences:
      if occurrence.kind.satisfies(method):
        satisfying_occurrences.append(occurrence)

  return satisfying_occurrences


def test_get_symbols() -> None:
  """Unit tests for `get_symbols`."""

  table: SymbolTable = build_symbol_table(sample_document)

  decl = OccurrenceKind.DECLARATION
  defn = OccurrenceKind.DEFINITION
  ref = OccurrenceKind.REFERENCE

  def r(a: int, b: int, c: int, d: int) -> Range:
    return Range(Position(a,b), Position(c,d))

  expect_eq(get_symbols(declMethod, table, Position(0,0)),
    [Occurrence("one", decl, r(0, 0, 0, 3))])

  expect_eq(get_symbols(defnMethod, table, Position(0,0)),
    [Occurrence("one", defn, r(3, 0, 3, 3))])

  expect_eq(get_symbols(refsMethod, table, Position(0,0)),
    [
      Occurrence("one", decl, r(0, 0, 0, 3)),
      Occurrence("one", ref,  r(1, 0, 1, 3)),
      Occurrence("one", defn, r(3, 0, 3, 3)),
    ])

  expect_eq(get_symbols(declMethod, table, Position(0,3)),
    [])

  expect_eq(get_symbols(declMethod, table, Position(5,1)),
    [Occurrence("two", decl, r(0, 4, 0, 7))])

  expect_eq(get_symbols(defnMethod, table, Position(5,1)),
    [Occurrence("two", defn, r(5, 0, 5, 3))])

  expect_eq(get_symbols(refsMethod, table, Position(0,6)),
    [
      Occurrence("two", decl, r(0, 4, 0, 7)),
      Occurrence("two", defn, r(5, 0, 5, 3)),
    ])


def get_locations(
  uri: str,
  method: str,
  table: SymbolTable,
  pos: Position) -> List[Dict[str, Any]]:
  """Get the LSP reply for a symbol query."""

  occurrences: List[Occurrence] = get_symbols(method, table, pos)

  return [
    {
      "uri": uri,
      "range": occurrence.range.to_dict()
    }
    for occurrence
    in occurrences
  ]


def test_get_locations() -> None:
  """Unit tests for `get_locations`."""

  table: SymbolTable = build_symbol_table(sample_document)
  uri: str = "myURI"

  expect_eq(
    get_locations(uri, refsMethod, table, Position(0,0)),
    [
      {
        "uri": "myURI",
        "range": {
          "start": { "line": 0, "character": 0 },
          "end":   { "line": 0, "character": 3 },
        }
      },
      {
        "uri": "myURI",
        "range": {
          "start": { "line": 1, "character": 0 },
          "end":   { "line": 1, "character": 3 },
        }
      },
      {
        "uri": "myURI",
        "range": {
          "start": { "line": 3, "character": 0 },
          "end":   { "line": 3, "character": 3 },
        }
      },
    ])


def handle_symbol_query(
  method: str,
  msg_id: Optional[int],
  params: dict[str, Any]) -> None:

  """Handle a query for symbol information."""

  uri: str = params["textDocument"]["uri"]
  pos: Position = Position.from_dict(params["position"])

  contents: str = documents.get(uri, (-1, "<no doc>"))[1]

  table: SymbolTable = build_symbol_table(contents)

  locations: List[Dict[str, Any]] = (
    get_locations(uri, method, table, pos))

  send_reply(msg_id, locations)


# ---------------------- Language Server Protocol ----------------------
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


def send_reply(msg_id: Optional[int], result: Any) -> None:
  """Send one LSP reply."""

  if msg_id is None:
    complain("Missing message ID in request.")
  else:
    send_message({"jsonrpc": "2.0", "id": msg_id, "result": result})


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
      rng: Range = Range.from_dict(change["range"])

      def offset(pos: Position) -> int:
        """Convert line/character to byte offset into text."""

        lines = split_lines(text)

        if not 0 <= pos.line < len(lines):
          raise RuntimeError("Position line out of range")

        if not 0 <= pos.character <= len(lines[pos.line]):
          raise RuntimeError("Position character out of range")

        line_start = sum(len(lines[i]) for i in range(pos.line))
        return line_start + pos.character

      start_off = offset(rng.start)
      end_off = offset(rng.end)
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
        "start": Position(start_line, start_character).to_dict(),
        "end": Position(end_line, end_character).to_dict(),
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

  try:
    while True:
      msg = read_message()
      if "method" not in msg:
        complain("Message is missing `method` attribute.")

      method = msg["method"]

      # Requests have an ID; notifications do not.
      msg_id: Optional[int] = msg.get("id")

      if method == "initialize":
        # This lets me test how the editor responds when the server is
        # slow to initialize.
        if delayMS := os.getenv("LSP_TEST_SERVER_INITIALIZE_DELAY_MS"):
          time.sleep(int(delayMS) / 1000)

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
        send_reply(msg_id, result)

      elif method == "initialized":
        initialized = True

      elif method == "shutdown":
        if not initialized:
          complain("Received `shutdown` without `initialized`.")
        shutdown_received = True
        send_reply(msg_id, None)

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
        send_reply(msg_id, {
          "uri": uri,
          "text": contents,
          "version": version
        })

      elif (method == declMethod or
            method == defnMethod or
            method == refsMethod):
        handle_symbol_query(method, msg_id, msg["params"]);

      else:
        if msg_id is not None:
          # For other requests, respond with empty result.
          send_reply(msg_id, None)
        else:
          # For other notifications, just accept and ignore.
          pass

  except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    if debugLevel >= 1:
      traceback.print_exc(file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
  test_Position()
  test_Range()
  test_split_lines()
  test_get_words()
  test_brace_on_line_or_next()
  test_build_symbol_table()
  test_get_symbols()
  test_get_locations()
  test_apply_text_edits()

  if os.getenv("UNIT_TEST"):
    # Only run the unit tests.
    pass
  else:
    main()


# EOF
