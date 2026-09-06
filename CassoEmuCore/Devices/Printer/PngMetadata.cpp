#include "Pch.h"

#include "Devices/Printer/PngMetadata.h"




// The printable Latin-1 ranges a PNG keyword may draw from. The gap between
// them (0x7F..0xA0) is where the specification's two printable runs stop, and
// a keyword must not reach into it.
static constexpr unsigned char  s_kPrintableLowFirst  = 32;    // space
static constexpr unsigned char  s_kPrintableLowLast   = 126;
static constexpr unsigned char  s_kPrintableHighFirst = 161;
static constexpr unsigned char  s_kPrintableHighLast  = 255;
static constexpr char           s_kSpace              = ' ';





////////////////////////////////////////////////////////////////////////////////
//
//  IsValidKeyword
//
//  The three ways a keyword goes wrong, in the order they are cheapest to
//  detect: a bad length, a character outside the printable runs, and a space
//  in a position the specification forbids.
//
//  The space rules exist because a keyword is compared byte for byte by
//  readers. A leading, trailing or doubled space is invisible in every tool
//  that displays one, so two keywords that look identical would not match --
//  which is exactly the kind of defect that survives a review.
//
////////////////////////////////////////////////////////////////////////////////

bool PngMetadata::IsValidKeyword (const string & keyword)
{
    bool            valid  = true;
    size_t          i      = 0;
    unsigned char   ch     = 0;
    size_t          length = keyword.length();



    valid = (length >= 1) && (length <= kMaxKeywordLength);

    if (valid)
    {
        valid = (keyword.front() != s_kSpace) && (keyword.back() != s_kSpace);
    }

    for (i = 0; valid && i < length; i++)
    {
        ch = (unsigned char) keyword[i];

        valid = ((ch >= s_kPrintableLowFirst)  && (ch <= s_kPrintableLowLast))
             || ((ch >= s_kPrintableHighFirst) && (ch <= s_kPrintableHighLast));

        if (valid && (i > 0))
        {
            valid = !((keyword[i] == s_kSpace) && (keyword[i - 1] == s_kSpace));
        }
    }

    return valid;
}
