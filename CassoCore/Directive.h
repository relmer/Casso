#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Directive
//
//  The assembler's directive vocabulary as tokens rather than strings. The
//  parser resolves a spelling to one of these once; everything downstream
//  switches on the token and never compares a directive name again.
//
//  Two reasons this is a token and not a string:
//
//    * A switch over this enum is exhaustiveness-checked, so adding a
//      directive fails the build at every site that has to handle it. The
//      string form let a new directive be added to one dispatch chain and
//      silently missed by the other two.
//    * The name -> token table IS the assembler dialect. as65 spells a byte
//      directive DB / BYT / BYTE / FCB / FCC and ca65 spells it .byte; both
//      are the same token, so a second dialect is a second table rather than
//      a second copy of the assembler.
//
//  Segment spellings collapse deliberately: .CODE and .SEGMENT_CODE are one
//  token, because every consumer already treated them identically.
//
////////////////////////////////////////////////////////////////////////////////

enum class Directive
{
    None = 0,       // the line is not a directive

    Align,
    Byte,
    Cmap,
    Dd,
    Ds,
    Else,
    End,
    Endif,
    Error,
    If,
    Ifdef,
    Ifndef,
    Include,
    List,
    MultiNop,
    Nolist,
    OptNoop,
    Org,
    Page,
    SegmentBss,
    SegmentCode,
    SegmentData,
    Struct,
    Text,
    Title,
    Word,

    //  Introduced by the Merlin dialect. These have NO as65 spelling and must
    //  never acquire one -- FR-005 forbids admitting one dialect's constructs
    //  into another, so `.HEX` staying unrecognized by as65 is the requirement
    //  rather than an omission.
    //
    //  That makes this enum no longer total over as65's spelling table, which
    //  it was when as65 was the only dialect. The totality that still holds, and
    //  that DialectMechanismTests now checks, is weaker and correct: every token
    //  is claimed by AT LEAST ONE dialect. A token claimed by none is
    //  unreachable, which is the bug the original sweep existed to catch.
    StringData,             // ASC / DCI / INV / FLS / STR / REV -- one operation, six encodings
    HexData,                // HEX
    WordHighFirst,          // DDB -- the byte order Word does not do
    ErrorIf,                // ERR -- fails the assembly when its expression is non-zero
    Loop,                   // LUP
    LoopEnd,                // --^
    DummySection,           // DUM -- assigns addresses, emits nothing
    DummySectionEnd,        // DEND
    MacroDef,               // MAC
    MacroEnd,               // <<< -- the TERMINATOR, not an invocation
    CpuSelect,              // XC
    ObjectFile,             // DSK
    KeyboardInput,          // KBD -- binds a symbol from an answer supplied to the assembly

    //  Recognized ONLY so they can be refused by name. A construct outside the
    //  supported subset must say so; failing as an unknown directive reads as
    //  "Merlin support is broken" rather than "this is outside the subset".
    Relocatable,            // REL
    EntrySymbol,            // ENT
    ExternalSymbol,         // EXT
    FileType,               // TYP
    SaveObject,             // SAV

    Count,          // sentinel: sizes token-indexed tables
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable
//
//  One spelling -> token mapping. Holding both the dotted canonical names and
//  the bare as65 synonyms in a single table is what lets the parser do one
//  lookup instead of walking an if/else chain per form.
//
////////////////////////////////////////////////////////////////////////////////

class DirectiveTable
{
public:
    // One row of the table. Nested rather than declared in the .cpp: a bare
    // struct there has external linkage and no keyword can change that.
    struct Spelling
    {
        const char *  name;
        Directive     token;
    };

    // `word` is expected upper-cased, with or without its leading dot.
    // Returns Directive::None when the spelling is not a directive.
    static Directive  FromSpelling (const std::string & word);

    // Only the spellings that are ambiguous with an instruction and so cannot
    // sit in the main table -- today just as65's RMB, which is .DS given one
    // operand and the Rockwell bit instruction given two. Returns None for
    // everything else, ordinary directives included, so a caller that has
    // already missed in FromSpelling can check here without rescanning.
    //
    // The caller owns the disambiguation: resolving RMB without first ruling
    // the instruction out would turn `rmb 3,$20` into storage.
    static Directive  FromAmbiguousSpelling (const std::string & word);

    // FromSpelling, falling back to FromAmbiguousSpelling. For callers whose
    // context rules the instruction out outright -- inside a .STRUCT body every
    // member declaration names a storage directive, so nothing is ambiguous.
    static Directive  FromStorageSpelling (const std::string & word);

    // The canonical dotted spelling, for diagnostics and listings.
    static const char * GetCanonicalName (Directive directive);

    // Every accepted spelling, so DirectiveTokenTests can sweep the whole
    // vocabulary instead of a hand-picked sample -- a spelling added to the
    // table is covered without anyone editing a test.
    static std::span<const Spelling> GetAllSpellings();
};
