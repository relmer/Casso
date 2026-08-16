#include "Pch.h"

#include "StringEncoding.h"



//  The one delimiter that selects low ASCII. Every other character sets the
//  high bit.
static const char  s_kLowAsciiDelimiter = '\'';

//  Apple II screen codes: inverse occupies $00-$3F and flashing $40-$7F, so both
//  are reached by masking to six bits and then choosing the bank.
static const Byte  s_kScreenCodeMask   = 0x3F;
static const Byte  s_kFlashingBank     = 0x40;

static const Byte  s_kHighBit          = 0x80;
static const Byte  s_kSevenBitMask     = 0x7F;





////////////////////////////////////////////////////////////////////////////////
//
//  StringEncoding::HighBitFromDelimiter
//
//  Merlin reads the encoding off the quoting rather than off a second
//  directive, so the delimiter is data about the string and not punctuation.
//
//  Every delimiter sets the high bit EXCEPT the apostrophe. That is the shape
//  the evidence supports and the obvious alternative does not: `"` ($22) and `!`
//  ($21) both produce high ASCII in the vendor objects, which rules out any rule
//  ordering delimiters by ASCII value -- `!` sorts below `'` and would have to
//  give low ASCII under it.
//
//  UNVERIFIED, deliberately flagged: no `'`-delimited string appears anywhere in
//  the corpus, so the low-ASCII half of this rule rests on documentation rather
//  than on bytes. It is the conservative choice -- it changes nothing about any
//  string the corpus DOES contain -- but it is a capture question, not a settled
//  one.
//
////////////////////////////////////////////////////////////////////////////////

bool StringEncoding::HighBitFromDelimiter (char delimiter)
{
    return (delimiter != s_kLowAsciiDelimiter);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StringEncoding::Encode
//
//  Text to bytes.
//
//  DCI is the mode worth reading twice. It marks the END of the string by
//  INVERTING the high bit of the last character rather than by appending a
//  terminator, so the string costs no extra byte and the reader detects the end
//  by watching the bit change. Verified directly: `DCI "0200IN"` in LABELS.S
//  emits B0 B2 B0 B0 C9 4E, where every byte carries the high bit except the
//  final `N`.
//
//  Inverting rather than clearing matters. Under a low-ASCII delimiter the last
//  character would need its bit SET, and an implementation that cleared it would
//  agree with the corpus on every entry -- because every DCI on the disk is
//  high-ASCII -- while being wrong on the case nobody wrote down.
//
////////////////////////////////////////////////////////////////////////////////

void StringEncoding::Encode (
    const std::string          &  text,
    StringEncodingMode            mode,
    bool                          highBit,
    std::vector<Byte>          &  outBytes)
{
    std::string  ordered = text;
    size_t       start   = 0;
    size_t       i       = 0;
    Byte         value   = 0;

    if (mode == StringEncodingMode::Reversed)
    {
        std::reverse (ordered.begin(), ordered.end());
    }

    if (mode == StringEncodingMode::LengthPrefixed)
    {
        // UNVERIFIED: no STR appears in the corpus. The count byte is written
        // plain, since it is a length rather than a character.
        outBytes.push_back (static_cast<Byte> (ordered.size() & 0xFF));
    }

    start = outBytes.size();

    for (i = 0; i < ordered.size(); i++)
    {
        value = static_cast<Byte> (ordered[i]) & s_kSevenBitMask;

        switch (mode)
        {
            case StringEncodingMode::Inverse:
                // UNVERIFIED: INV appears once in the corpus, in a linker demo
                // that ships no object, so this cannot be checked against bytes.
                value = value & s_kScreenCodeMask;
                break;

            case StringEncodingMode::Flashing:
                // UNVERIFIED: FLS appears nowhere in the corpus.
                value = (value & s_kScreenCodeMask) | s_kFlashingBank;
                break;

            default:
                if (highBit)
                {
                    value = value | s_kHighBit;
                }

                break;
        }

        outBytes.push_back (value);
    }

    // The terminator is the LAST character with its high bit flipped, so an
    // empty string has nothing to mark and must not touch the byte before it.
    if ((mode == StringEncodingMode::DciTerminated) && (outBytes.size() > start))
    {
        outBytes.back() = outBytes.back() ^ s_kHighBit;
    }
}
