#pragma once

#include "../EmuTests/IFixtureProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixtureFile
//
//  One decoded DOS 3.3 binary file: where it loaded, and what it contained.
//
//  The load address is kept rather than discarded because it is evidence. Every
//  committed object records the address Merlin assembled it to, and an entry
//  claiming "984 bytes at $8000" can be checked against the file itself instead
//  of against a note in the spec.
//
////////////////////////////////////////////////////////////////////////////////

struct MerlinFixtureFile
{
    Word                 loadAddress = 0;
    std::vector<Byte>    payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture
//
//  Decodes the vendor fixtures under UnitTest/Fixtures/Merlin/, which are raw
//  DOS 3.3 files exactly as they sat on the Merlin Pro 2.23 disk.
//
//  The convention lives here ONCE rather than in every entry. Repeating a
//  four-byte header strip and a high-bit mask per entry is how one entry ends up
//  decoding differently from the rest, and a decoding difference produces bytes
//  that are wrong without looking wrong.
//
//  THIS CODE MUST NEVER ASSERT THAT BIT 7 IS SET. Two separate conventions break
//  that assumption, and both are load-bearing:
//
//    - Merlin stores source as high-bit ASCII EXCEPT for spaces, which are plain
//      $20. LABELS.S alone contains 81 of them. A decoder validating the high bit
//      would fail on the first space of every file.
//    - DCI, in object output, marks its terminator by CLEARING the high bit --
//      the exact case a chunk of this corpus exists to pin. An assertion here
//      would reject the evidence it was meant to protect.
//
//  Masking is unconditional for that reason: it is correct whether or not the
//  bit was set, where a check would have to know which convention applied.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinFixture
{
public:
    //  An object file -- the bytes Merlin emitted, compared against byte for
    //  byte. No transformation beyond removing the header.
    static HRESULT           LoadObject (
        IFixtureProvider           &  provider,
        const std::string          &  relativePath,
        MerlinFixtureFile          &  outFile);

    //  A source file, decoded to ordinary text: high bits masked off and Merlin's
    //  CR line terminators translated to newlines.
    static HRESULT           LoadSource (
        IFixtureProvider           &  provider,
        const std::string          &  relativePath,
        std::string                &  outText);

private:
    static HRESULT           StripHeader (
        const std::vector<Byte>    &  raw,
        MerlinFixtureFile          &  outFile);
};
