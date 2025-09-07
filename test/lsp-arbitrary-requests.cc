// lsp-arbitrary-requests.cc
// A file with arbitrary requests to experiment with.

// Comment above `x`.
int x;

/*
  {
    "method": "textDocument/hover"
    "params": {
      "textDocument": {
        "uri": CUR_FILE_URI
      }
      "position": {
        "line": 4
        "character": 4
      }
    }
  }

  {
    "method": "textDocument/diagnostic"
    "params": {
      "textDocument": {
        "uri": CUR_FILE_URI
      }
    }
  }

  {
    "method": "$/methodNotFound"
    "params": null
  }

  {
    "method": "$/invalidRequest"
    "params": null
  }
*/

// EOF
