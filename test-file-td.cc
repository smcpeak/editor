// test-file-td.cc
// Tests for 'file-td' module.

#include "file-td.h"                   // this module

#include "nonport.h"                   // fileOrDirectoryExists, removeFile
#include "test.h"                      // USUAL_MAIN

#include <fstream>                     // ofstream

using std::ofstream;


static void testWhenUntitledExists()
{
  FileTextDocument file;
  file.filename = "untitled.txt";
  file.isUntitled = true;     // just being explicit

  // Create a file with that name.
  bool created = false;
  if (!fileOrDirectoryExists(file.filename.c_str())){
    ofstream of(file.filename.c_str());
    created = true;
  }

  // The document should still regard itself as not having a stale
  // modification time because it is untitled, hence not really
  // associated with any on-disk file.
  xassert(!file.hasStaleModificationTime());

  if (created) {
    (void)removeFile(file.filename.c_str());
  }
}


class TestTDO : public TextDocumentObserver {
public:      // data
  // Number of calls to 'observeTotalChange;
  int m_totalChanges;

public:      // funcs
  TestTDO()
    : TextDocumentObserver(),
      m_totalChanges(0)
  {}

  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override
  {
    m_totalChanges++;
  }
};


// Make sure that reading a file broadcasts 'observeTotalChange'.
static void testReadFile()
{
  FileTextDocument file;
  file.filename = "td.h";
  file.isUntitled = false;
  file.readFile();

  TestTDO ttdo;
  file.addObserver(&ttdo);
  file.readFile();
  file.removeObserver(&ttdo);

  xassert(ttdo.m_totalChanges == 1);
}


static void entry()
{
  testWhenUntitledExists();
  testReadFile();

  xassert(FileTextDocument::s_objectCount == 0);
  xassert(TextDocument::s_objectCount == 0);

  cout << "test-file-fd passed\n";

}

USUAL_TEST_MAIN


// EOF
