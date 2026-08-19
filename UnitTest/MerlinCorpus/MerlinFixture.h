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
//  Reads the vendor fixtures under UnitTest/Fixtures/Merlin/.
//
//  The two halves are committed in different forms, and the difference is
//  deliberate:
//
//    - OBJECTS are the raw DOS 3.3 files, byte for byte as they sat on the
//      Merlin Pro 2.23 disk, 4-byte header and all. They are the expectation an
//      assembly is compared against, so any transformation applied to them
//      would be a transformation applied to the answer.
//    - SOURCES are ordinary text files -- seven-bit ASCII with CRLF line
//      endings, no header. The transcoding happens ONCE, in
//      scripts/ExtractMerlinFixtures.ps1, rather than on every read here.
//
//  Sources are transcoded because they are INPUT. Casso's own file reader takes
//  text off the host filesystem, so a fixture stored as Apple II text is a file
//  the tool under test cannot open -- the corpus could only ever be assembled by
//  the test project, through a decoder no shipped code path uses. Committed as
//  text, the same file is what the unit tests read and what `CassoCli merlin`
//  reads, and the two cannot diverge.
//
//  THE TRANSCODING IS LOSSLESS FOR EVERYTHING THE ASSEMBLER CAN SEE. Across all
//  ten sources the only bytes that were ever below $80 are spaces, so masking
//  bit 7 changed no character's identity; every line ended in a single CR, and
//  every CR became a CRLF that the parser accepts identically. What it does
//  discard is the stored spelling of a space: a space separating fields carried
//  the high bit ($A0) where a space inside comment text did not ($20), and both
//  are now $20.
//
//  THAT LOSS IS THE POINT, not a regret. The distinction looked like a free
//  lexer -- the encoding marking field structure apart from comment text -- and
//  it was a trap, because source reaching Casso by any other route carries no
//  such distinction, and a parser leaning on it would work only on files
//  authored on a Merlin disk. It was never grammar. Now it cannot be mistaken
//  for grammar, because it is not there to lean on.
//
//  THE OBJECT PATH MUST NEVER ASSERT THAT BIT 7 IS SET. DCI marks its
//  terminator by CLEARING the high bit, so a low byte in an object is
//  meaningful data rather than an encoding artifact, and an assertion here
//  would reject the evidence the corpus exists to pin.
//
//  The verbatim disk bytes are not lost by this: their SHA-256 hashes stay
//  recorded in UnitTest/Fixtures/Merlin/README.md, and the extraction script
//  reproduces either form from the hash-pinned disk.
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

    //  A source file, which is already text. Read and handed over unmodified --
    //  no masking, no header, no line-ending translation, and no trailing-blank
    //  trimming. The corpus compares what Merlin assembled, so a reader that
    //  tidied its input would be testing the tidied text rather than the
    //  vendor's, and the field model this dialect turns on is made of exactly
    //  the whitespace such a step would remove.
    static HRESULT           LoadSource (
        IFixtureProvider           &  provider,
        const std::string          &  relativePath,
        std::string                &  outText);

private:
    static HRESULT           StripHeader (
        const std::vector<Byte>    &  raw,
        MerlinFixtureFile          &  outFile);
};
