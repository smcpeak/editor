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

  {
    "method": "$/memoryUsage"
    "params": null
  }

  [
    {
      "method": "textDocument/didChange"
      "params": {
        "contentChanges": [
          {
            "range": {
              "end": {"character":0 "line":87}
              "start": {"character":0 "line":87}
            }
            "text": "x"
          }
        ]
        "textDocument": {
          "uri": CUR_FILE_URI
          "version": CUR_FILE_VERSION
        }
        "wantDiagnostics": true
      }
    }
    {
      "method": "textDocument/didChange"
      "params": {
        "contentChanges": [
          {
            "range": {
              "end": {"character":1 "line":87}
              "start": {"character":0 "line":87}
            }
            "text": ""
          }
        ]
        "textDocument": {
          "uri": CUR_FILE_URI
          "version": CUR_FILE_VERSION
        }
        //"wantDiagnostics": true
      }
    }
  ]

*/

// EOF
