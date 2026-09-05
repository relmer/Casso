#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MetadataEntry
//
//  One PNG tEXt chunk: a keyword and its value.
//
//  IT LIVES BESIDE ITS CODEC, not beside the screenshot code that composes
//  the entries. PngCodec is the consumer that cannot do without it, while a
//  composer is one of several things that might produce entries; defining it
//  under Capture/ would make the printer's codec depend on the screenshot
//  directory and invert the layering.
//
//  Both fields are ASCII. tEXt values are Latin-1 by specification, and
//  nothing Casso emits needs a character outside ASCII -- which keeps the
//  chunks readable by every tool that has ever read a PNG, rather than the
//  smaller set that handles iTXt.
//
////////////////////////////////////////////////////////////////////////////////

struct MetadataEntry
{
    string  keyword;
    string  value;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PngMetadata
//
//  The PNG specification's rules about what a keyword may be, in one place
//  so the codec and the composers agree on them.
//
////////////////////////////////////////////////////////////////////////////////

class PngMetadata
{
public:
    static constexpr size_t  kMaxKeywordLength = 79;

    // The specification's keyword rules: 1 to 79 printable Latin-1
    // characters, with no leading, trailing, or consecutive spaces.
    //
    // A caller handing over a keyword that fails this has a bug, not bad
    // input -- every keyword Casso writes is a compile-time constant -- so
    // the codec asserts rather than quietly dropping the entry.
    static bool  IsValidKeyword (const string & keyword);
};
