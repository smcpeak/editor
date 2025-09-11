// byte-or-column-count.h
// `ByteOrColumnCount` type.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_BYTE_OR_COLUMN_COUNT_H
#define EDITOR_BYTE_OR_COLUMN_COUNT_H

/* This type is used for data that represents either a byte count or a
   column count, depending on the context.

   For the moment, I just want a distinct type name to document the
   role.  I'm thinking I would like to change the places that use this
   to be able to statically distinguish the two possibilities.
*/
using ByteOrColumnCount = int;

#endif // EDITOR_BYTE_OR_COLUMN_COUNT_H
