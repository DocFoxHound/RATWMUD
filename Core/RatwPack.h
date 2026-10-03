#pragma once
// Binary snapshots (Docs/Design/31-responsiveness.md, Phase 4.8): the same tree of values a snapshot's JSON holds, in
// a compact binary form that is quicker to write and read (no numbers turned into text and back) and smaller. The
// browser client reads it into the very same objects (Client/src/net/pack.ts), so nothing that uses a snapshot changes.
//
// A value is a tag byte, then:
//   0 null, 1 false, 2 true
//   3 a whole number: zigzag varint          4 a float32          5 a float64
//   6 a string                               7 an array: varint count, then the values
//   8 an object: varint count, then for each field its key (a string) and its value
// A string is a varint n: n even, the first time it appears: n/2 bytes of UTF-8 follow, and it takes the next number
// in the message's table; n odd, a string met before: number (n - 1)/2 in that table. Keys and values share one table.
#include "RatwJsonDoc.h"

#include <string>

namespace ratw::pack
{
std::string encode(const json::Value& v);
// False for anything malformed (or nested deeper than 256).
bool decode(const std::string& bytes, json::Value& out);
} // namespace ratw::pack
