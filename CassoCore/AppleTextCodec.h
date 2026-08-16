#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodec
//
//  Host text to Apple II text and back.
//
//  THE HIGH-BIT CONVENTION IS A PARAMETER, NOT A CONSTANT, and deliberately so.
//  DOS 3.3 sequential text is high-bit-set ASCII terminated by $8D -- the native
//  Apple II character convention. ProDOS TXT files are conventionally plain
//  seven-bit ASCII with $0D, high bit clear, so they interchange with other
//  systems. That difference is asserted often enough to be worth believing, but
//  it has NOT been verified against a real ProDOS text file here, and the cost
//  of being wrong is nasty: every character off by $80 reads as garbage rather
//  than as an obvious bug in a converter.
//
//  Parameterizing costs one argument and makes the answer a data change rather
//  than a rewrite. Settling it needs a ProDOS volume carrying a real TXT file --
//  dump its first bytes and read the convention off the disk.
//
//  Note what is NOT evidence for this: DOS 3.3 stores catalog NAMES in high
//  ASCII, which is verifiable on any master image. That says nothing about file
//  CONTENTS, and generalizing from the adjacent case is exactly how a comment
//  in the nibblization tests came to license a data-loss defect.
//
//  ROUND-TRIP IDENTITY HOLDS IN ONE DIRECTION ONLY, and the asymmetry is
//  deliberate rather than incidental:
//
//      host -> Apple -> host   IDENTITY. This is what putting a file on a disk
//                              and reading it back must preserve, and it is
//                              what the tests pin.
//
//      Apple -> host -> Apple  NOT identity. It NORMALIZES: a plain $20 comes
//                              back as $A0, a plain $0D as $8D, a CR/LF pair as
//                              a single terminator.
//
//  Decoding is deliberately many-to-one -- tolerating mixed high and low bytes
//  is what lets a real vendor file be read at all -- and many-to-one cannot be
//  inverted. The consequence is user-visible: extracting a text file and
//  placing it back rewrites bytes that were never edited. On a real macro
//  library that is 37 low-ASCII bytes changed in a file the user may care about
//  preserving.
//
//  The lossless path is therefore NOT this class. A caller that wants
//  byte-exact round-tripping asks for no conversion at all, which the volume
//  layer provides by default -- reads return raw bytes and this codec is only
//  invoked when a caller explicitly selects a text encoding.
//
////////////////////////////////////////////////////////////////////////////////

enum class AppleTextConvention
{
    HighAscii,    // High bit SET, $8D terminator -- DOS 3.3 sequential text.
    PlainAscii,   // High bit CLEAR, $0D terminator -- ProDOS TXT convention.
};



class AppleTextCodec
{
public:
    //  Host text to Apple text. Accepts LF or CRLF line endings and emits the
    //  convention's single-character terminator.
    //
    //  FAILS on any character with no Apple II representation rather than
    //  dropping it or masking it to seven bits. A smart quote or an em-dash
    //  from a modern editor would otherwise become a different, plausible
    //  character -- a conversion reporting success while changing the text.
    //  outBadOffset names the offending byte so the message can point at it.
    static HRESULT  Encode (const std::string        & hostText,
                            AppleTextConvention        convention,
                            std::vector<Byte>        & outBytes,
                            size_t                   & outBadOffset);

    //  Apple text to host text, with the terminator normalized to LF. A
    //  trailing terminator is preserved as a trailing newline, because a file
    //  that ended with one and a file that did not are different files.
    static void     Decode (const std::vector<Byte>  & appleBytes,
                            AppleTextConvention        convention,
                            std::string              & outHostText);

    //  The terminator this convention writes.
    static Byte     GetTerminator (AppleTextConvention convention);

private:
    //  Printable ASCII plus tab. Everything else has no Apple II text meaning.
    static bool  IsRepresentable (char c);
};
