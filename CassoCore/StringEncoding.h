#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StringEncodingMode
//
//  How a run of text becomes bytes.
//
//  One operation with six settings rather than six operations: every mode takes
//  the same text and produces bytes from it, differing only in high-bit
//  handling, ordering, and what is added at the ends. That is why the parser
//  carries ONE directive token for all six spellings.
//
//  EVIDENCE STATUS, which is not uniform and must not be presented as if it
//  were. Counts are string-directive lines in the five committed sources that
//  ship an object to check against:
//
//    Plain           ASC    24 lines -- VERIFIED against object code
//    DciTerminated   DCI   130 lines -- VERIFIED, and the corpus's sharpest probe
//    Reversed        REV     1 line  -- VERIFIED (`REV "CALL-151"` emits 151-LLAC)
//    Inverse         INV     0 lines -- UNVERIFIED
//    Flashing        FLS     0 lines -- UNVERIFIED
//    LengthPrefixed  STR     0 lines -- UNVERIFIED
//
//  INV appears once in the whole corpus, in PI.START.S, which is a linker demo
//  and ships no object -- so it cannot be checked. FLS and STR appear nowhere at
//  all. The three unverified modes below implement the documented rule and are
//  labeled as such at each one. They are the reason the spec's corpus floor
//  demands one captured entry PER SPELLING rather than one for the family: for
//  half of this table, capture is the only way to know.
//
////////////////////////////////////////////////////////////////////////////////

enum class StringEncodingMode
{
    Plain,              // ASC
    DciTerminated,      // DCI -- the last character's high bit is inverted
    Inverse,            // INV
    Flashing,           // FLS
    LengthPrefixed,     // STR -- a leading count byte
    Reversed,           // REV -- the text is emitted back to front
};





////////////////////////////////////////////////////////////////////////////////
//
//  StringEncoding
//
//  Text to bytes, for every dialect that needs encoded string data.
//
//  This is the highest-risk area in the Merlin dialect, and the risk has a
//  specific shape: a high-bit or terminator error produces output that still
//  looks entirely plausible on inspection. Nothing crashes, no diagnostic
//  fires, and a hex dump reads like text. Only a byte comparison against real
//  object code catches it, which is why the corpus exists.
//
////////////////////////////////////////////////////////////////////////////////

class StringEncoding
{
public:
    //  Whether the text carries the high bit, decided by the character that
    //  DELIMITED it. Merlin infers the encoding from the quoting rather than
    //  from a separate directive, so this is part of resolving the mode.
    static bool  IsHighBitDelimiter (char delimiter);

    //  `text` is the payload between the delimiters, already stripped of them.
    //  Bytes are APPENDED, so a caller assembling a line need not manage a
    //  temporary.
    static void  Encode (
        const std::string          &  text,
        StringEncodingMode            mode,
        bool                          highBit,
        std::vector<Byte>          &  outBytes);
};
