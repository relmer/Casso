#include "Pch.h"

#include "AppleTextCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodec::GetTerminator
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleTextCodec::GetTerminator (AppleTextConvention convention)
{
    return (convention == AppleTextConvention::HighAscii) ? (Byte) 0x8D : (Byte) 0x0D;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodec::IsRepresentable
//
//  Printable ASCII and tab. A character outside that has no Apple II text
//  meaning, and there is no safe fallback: masking the high bit off a UTF-8
//  byte silently produces a DIFFERENT letter, which is worse than refusing.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleTextCodec::IsRepresentable (char c)
{
    unsigned char  u = (unsigned char) c;



    return (u >= 0x20 && u <= 0x7E) || u == 0x09;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodec::Encode
//
//  CRLF collapses to one terminator; a lone LF becomes one; a lone CR is
//  treated as a line ending too, so text from any of the three host
//  conventions converts the same way.
//
//  A character with no Apple II representation fails the whole conversion. The
//  alternative -- dropping it, or masking it into the ASCII range -- writes a
//  file that differs from the source while reporting success, which is the
//  failure shape this project keeps finding rather than a convenience.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleTextCodec::Encode (
    const std::string    &  hostText,
    AppleTextConvention     convention,
    std::vector<Byte>    &  outBytes,
    size_t               &  outBadOffset)
{
    HRESULT  hr         = S_OK;
    Byte     terminator = GetTerminator (convention);
    Byte     highBit    = (convention == AppleTextConvention::HighAscii) ? (Byte) 0x80 : (Byte) 0x00;
    size_t   i          = 0;
    bool     ok         = true;



    outBytes.clear();
    outBadOffset = 0;

    for (i = 0; i < hostText.size(); i++)
    {
        char  c = hostText[i];

        if (c == '\r')
        {
            // Swallow the LF of a CRLF pair so it yields one line, not two.
            bool  isCrLf = (i + 1) < hostText.size() && hostText[i + 1] == '\n';

            outBytes.push_back (terminator);

            if (isCrLf)
            {
                i++;
            }

            continue;
        }

        if (c == '\n')
        {
            outBytes.push_back (terminator);
            continue;
        }

        ok = IsRepresentable (c);

        if (!ok)
        {
            outBadOffset = i;
            break;
        }

        outBytes.push_back ((Byte) ((Byte) c | highBit));
    }

    CBREx (ok, E_INVALIDARG);

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodec::Decode
//
//  The terminator becomes LF; every other byte drops its high bit. A trailing
//  terminator is kept as a trailing newline: a file that ended with one and a
//  file that did not are different files, and quietly agreeing them would make
//  the round trip lossy in the one place users notice.
//
//  THE HIGH BIT IS STRIPPED IF PRESENT AND TOLERATED IF ABSENT -- never
//  validated. Real vendor files are mixed: Merlin's T.MACRO LIBRARY is 1,578 of
//  1,616 bytes high-bit, with 37 plain $20 spaces among 236 high $A0 ones. A
//  decoder that required bit 7 would reject a genuine file, and one that
//  compared the raw byte to $8D would miss a plain $0D line ending in the same
//  file. Comparing after stripping handles both, which is why the terminator
//  test happens on the seven-bit value.
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextCodec::Decode (
    const std::vector<Byte>  &  appleBytes,
    AppleTextConvention         convention,
    std::string              &  outHostText)
{
    constexpr Byte  kLineFeed = 0x0A;

    //  Stripped to seven bits, so it matches whichever form the file carries.
    //  Both conventions reduce to the same value here, which is a consequence
    //  of tolerating mixed bytes rather than a coincidence: once the high bit
    //  is ignored, high-ASCII and plain-ASCII text differ in nothing.
    Byte    terminator = (Byte) (GetTerminator (convention) & 0x7F);
    size_t  i          = 0;



    outHostText.clear();

    for (i = 0; i < appleBytes.size(); i++)
    {
        Byte  stripped = (Byte) (appleBytes[i] & 0x7F);

        if (stripped == terminator)
        {
            // A line feed following a carriage return belongs to it. Apple II
            // convention is CR-only, but a file that arrived from elsewhere
            // carries both, and emitting two newlines would double every blank
            // line in it.
            bool  hasLineFeed = (i + 1) < appleBytes.size()
                             && (Byte) (appleBytes[i + 1] & 0x7F) == kLineFeed;

            outHostText += '\n';

            if (hasLineFeed)
            {
                i++;
            }

            continue;
        }

        if (stripped == kLineFeed)
        {
            // A lone line feed is still a line ending. Passing it through as a
            // literal would put a raw newline in the host text anyway, so it
            // may as well be one deliberately.
            outHostText += '\n';
            continue;
        }

        outHostText += (char) stripped;
    }
}
