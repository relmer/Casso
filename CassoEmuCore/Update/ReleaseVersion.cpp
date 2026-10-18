#include "Pch.h"

#include "Update/ReleaseVersion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::Parse
//
//  Accepts "1.30.0" or "v1.30.0" and nothing else: three decimal parts, no
//  suffix, no surrounding space. Anything else is ERROR_INVALID_DATA.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReleaseVersion::Parse (std::string_view text, ReleaseVersion & outVersion)
{
    HRESULT  hr       = S_OK;
    bool     isParsed = false;



    isParsed = TryParse (text, outVersion);
    CBREx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseVersion::TryParse (std::string_view text, ReleaseVersion & outVersion)
{
    ReleaseVersion  parsed;
    bool            isValid = false;



    outVersion = {};

    if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
    {
        text.remove_prefix (1);
    }

    isValid = TryParseNumber (text, parsed.major) &&
              TryConsumeDot  (text)               &&
              TryParseNumber (text, parsed.minor) &&
              TryConsumeDot  (text)               &&
              TryParseNumber (text, parsed.patch) &&
              text.empty();

    if (isValid)
    {
        outVersion = parsed;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::TryParseNumber
//
//  Consumes one run of decimal digits from the front of `text`. Fails on an
//  empty run or one too long to be a version part.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseVersion::TryParseNumber (std::string_view & text, int & outNumber)
{
    static constexpr size_t  kMaxDigits = 6;
    static constexpr int     kRadix     = 10;
    size_t                   digits     = 0;



    outNumber = 0;

    while (digits <= kMaxDigits && digits < text.size() && text[digits] >= '0' && text[digits] <= '9')
    {
        outNumber = outNumber * kRadix + (text[digits] - '0');
        digits++;
    }

    text.remove_prefix (digits);

    return digits > 0 && digits <= kMaxDigits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::TryConsumeDot
//
//  Consumes the '.' between two version parts.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseVersion::TryConsumeDot (std::string_view & text)
{
    bool  isDot = !text.empty() && text.front() == '.';



    if (isDot)
    {
        text.remove_prefix (1);
    }

    return isDot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::ToString
//
////////////////////////////////////////////////////////////////////////////////

std::string ReleaseVersion::ToString() const
{
    return std::format ("{}.{}.{}", major, minor, patch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion::CompareMinorLine
//
//  Negative, zero or positive as this version's (major, minor) is before, the
//  same as, or after `other`'s. The patch is ignored.
//
////////////////////////////////////////////////////////////////////////////////

int ReleaseVersion::CompareMinorLine (const ReleaseVersion & other) const
{
    int  result = 0;



    if (major != other.major)
    {
        result = (major < other.major) ? -1 : 1;
    }
    else if (minor != other.minor)
    {
        result = (minor < other.minor) ? -1 : 1;
    }

    return result;
}
