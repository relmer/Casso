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
//  that assumption. They live in different files and are unrelated:
//
//    - In SOURCE, spaces appear BOTH ways. A space separating fields carries the
//      high bit ($A0); a space inside comment text does not ($20). LABELS.S holds
//      214 of the former and 81 of the latter. Measured across all nine committed
//      sources, spaces are the ONLY bytes below $80 -- not one non-space low byte
//      in any of them. A decoder validating the high bit dies on the first
//      comment line of the first file.
//    - In OBJECTS, DCI marks its terminator by CLEARING the high bit, where the
//      cleared bit is meaningful data rather than an encoding artifact. An
//      assertion here would reject evidence the corpus exists to pin.
//
//  Masking is unconditional for that reason: correct whichever convention
//  applies, where a check would have to know which one it was looking at.
//
//  THE SOURCE DISTINCTION IS A TRAP FOR THE PARSER, and is deliberately not
//  surfaced by this API. The encoding happens to mark field structure apart from
//  comment text, which looks like a free lexer. It is not: source arriving by any
//  other route -- a host editor, a read off a disk image, a file Casso itself
//  writes -- carries no such distinction, and a parser leaning on it would work
//  only on files authored on a Merlin disk. It is an observation about these
//  bytes, never grammar. T.SENDMSG settles it: all 26 of its spaces are $A0 and
//  none are $20, so the distinction is not even present in every vendor file.
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

    //  A type-B source file, decoded to ordinary text: the four-byte header
    //  removed, high bits masked off, and Merlin's CR line terminators
    //  translated to newlines.
    static HRESULT           LoadSource (
        IFixtureProvider           &  provider,
        const std::string          &  relativePath,
        std::string                &  outText);

    //  A type-T source file. Same text decoding, but DOS 3.3 gives a text file
    //  NO header -- the bytes are the content from offset zero. The vendor macro
    //  libraries ship this way: T.SENDMSG begins with the literal characters
    //  "SE", not a load address.
    //
    //  Kept as a separate entry point rather than sniffed, because guessing the
    //  file type from its bytes is precisely the kind of inference that succeeds
    //  on the sample and fails on the next file. Using the wrong one is safe: the
    //  length check in the type-B path rejects a header that was never there
    //  (T.SENDMSG's first four bytes claim 50382 bytes of a 149-byte file), so
    //  the mistake surfaces as a failure rather than as text missing its first
    //  four characters.
    static HRESULT           LoadTextSource (
        IFixtureProvider           &  provider,
        const std::string          &  relativePath,
        std::string                &  outText);

private:
    static HRESULT           StripHeader (
        const std::vector<Byte>    &  raw,
        MerlinFixtureFile          &  outFile);

    static void              DecodeText (
        const std::vector<Byte>    &  payload,
        std::string                &  outText);
};
