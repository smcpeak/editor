// doc-type-detect.h
// Document type detection.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_DETECT_H
#define EDITOR_DOC_TYPE_DETECT_H

#include "doc-name-fwd.h"              // DocumentName


// True if `docName` looks like a diff file or process.
bool isDiffName(DocumentName const &docName);


#endif // EDITOR_DOC_TYPE_DETECT_H
