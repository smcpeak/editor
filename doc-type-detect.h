// doc-type-detect.h
// Document type detection.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_DETECT_H
#define EDITOR_DOC_TYPE_DETECT_H

#include "doc-name-fwd.h"              // DocumentName [n]
#include "doc-type-fwd.h"              // DocumentType [n]


// Determine the document type based on its name or command line.
// Return `DT_UNKNOWN` if it cannot be determined.
DocumentType detectDocumentType(DocumentName const &docName);


#endif // EDITOR_DOC_TYPE_DETECT_H
