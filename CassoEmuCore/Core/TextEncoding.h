#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding
//
//  Byte strings moved between code pages, for the one place it matters: the
//  boundary where text the program holds becomes text a person reads.
//
//  A NARROW STRING CARRIES NO RECORD OF WHICH CODE PAGE IT IS IN, which is the
//  whole problem. `main`'s argv arrives in the process's own narrow code page,
//  a WOZ image stores its metadata in UTF-8, and a console renders whatever
//  code page it was set to -- three answers, all of them bytes, none of them
//  self-describing. Handing one of them to a stream that assumes another is how
//  a disk named `Br<o-slash>derbund` echoes back as `Br?derbund`: nothing
//  converted it, and the console read CP-1252 bytes as UTF-8.
//
//  Solving it here rather than by re-encoding the source is deliberate. The
//  source files are CP-1252 and the /utf-8 compiler switch is not used, so the
//  fix has to be a conversion at the edge, applied to whatever the program
//  happens to be holding.
//
//  Everything is a static: there is no state to carry, and the code pages are
//  parameters rather than a configured mode so a caller cannot forget to say
//  which direction it means.
//
////////////////////////////////////////////////////////////////////////////////

class TextEncoding
{
public:
    //  The code page the console is currently rendering, and the one this
    //  process's narrow strings -- argv among them -- are written in.
    static unsigned  GetConsoleCodePage();
    static unsigned  GetNarrowCodePage();

    //  A byte string re-encoded from one code page to another.
    //
    //  A character the target cannot spell becomes `?`, which is what the
    //  console would have shown anyway; the alternative is dropping it, and a
    //  name silently missing a letter is harder to recognize than one with a
    //  question mark in it. Identical code pages return the input untouched, so
    //  the common case costs nothing and cannot lose anything.
    //
    //  Nothing here fails: text a code page cannot represent is a property of
    //  the text, not a caller's mistake, and a diagnostic that could not be
    //  printed because printing it failed is the worst possible outcome.
    static std::string  Convert (const std::string & text,
                                 unsigned            fromCodePage,
                                 unsigned            toCodePage);

    //  What a program holding narrow strings should write to the console. The
    //  two code pages agree far more often than not, in which case this is the
    //  identity.
    static std::string  NarrowToConsole (const std::string & text);

    //  UTF-8 data read out of a file format that specifies it -- WOZ META --
    //  brought into the narrow world the rest of the program's strings live in,
    //  so a diagnostic is in ONE code page by the time it reaches the boundary
    //  above rather than a mixture of two.
    static std::string  Utf8ToNarrow (const std::string & text);

    //  The crossings between the wide world the platform hands a GUI program
    //  and the narrow one every string in core lives in.
    //
    //  NARROW MEANS GetNarrowCodePage() -- the process code page, NOT UTF-8.
    //  Converting argv to UTF-8 here would parse beautifully and then fail at
    //  the first file open, because the narrow file APIs read those bytes as
    //  the process code page. These are exactly as lossy as the console
    //  tool's own argv and no lossier, and if the process code page ever
    //  becomes UTF-8, the same calls turn lossless with no edit here.
    static std::string   WideToNarrow (const std::wstring & text);
    static std::wstring  NarrowToWide (const std::string  & text);
};
