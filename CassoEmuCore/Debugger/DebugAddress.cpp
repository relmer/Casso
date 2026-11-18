#include "Pch.h"

#include "DebugAddress.h"





static constexpr HRESULT  s_kInvalidData   = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
static constexpr size_t   s_kMaxHexDigits  = 4;
static constexpr int      s_kHexDigitBits  = 4;
static constexpr int      s_kDecimalDigits = 10;
static constexpr char     s_kSystemPrefix  = 'S';
static constexpr char     s_kRomPrefix     = 'R';
static constexpr char     s_kHexPrefix     = '$';





////////////////////////////////////////////////////////////////////////////////
//
//  MakeBank
//
////////////////////////////////////////////////////////////////////////////////

DebugAddress DebugAddress::MakeBank (Byte bank, Word address)
{
    DebugAddress  result (address);



    result.m_space = DebugAddressSpace::Bank;
    result.m_bank  = bank;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeSystem
//
////////////////////////////////////////////////////////////////////////////////

DebugAddress DebugAddress::MakeSystem (Word address)
{
    DebugAddress  result (address);



    result.m_space = DebugAddressSpace::System;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRom
//
////////////////////////////////////////////////////////////////////////////////

DebugAddress DebugAddress::MakeRom (Word address)
{
    DebugAddress  result (address);



    result.m_space = DebugAddressSpace::Rom;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Matches
//
//  Whether a stored entry (this) stops on an access at a resolved address.
//  An entry in the current view matches the address in every bank; one with
//  a space matches only an access in that space and, for a bank, that bank.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugAddress::Matches (const DebugAddress & access) const
{
    if (m_address != access.m_address)
    {
        return false;
    }

    if (IsCurrentView())
    {
        return true;
    }

    return m_space == access.m_space && m_bank == access.m_bank;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Parse
//
//  `[space separator] hex16`: a bank digit 0-E, S or R, then the separator,
//  then one to four hex digits with an optional `$`. Without a space the
//  address is in the current view. Leading and trailing blanks are ignored.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugAddress::Parse (std::string_view text, char separator, DebugAddress & out)
{
    HRESULT       hr       = S_OK;
    size_t        first    = text.find_first_not_of (' ');
    size_t        last     = text.find_last_not_of (' ');
    DebugAddress  result;
    Word          address  = 0;
    bool          hasSpace = false;



    CBREx (first != std::string_view::npos, s_kInvalidData);

    text     = text.substr (first, last - first + 1);
    hasSpace = text.size() > 1 && text[1] == separator;

    if (hasSpace)
    {
        hr = ParseSpace (text[0], result);
        CHR (hr);

        text = text.substr (2);
    }

    hr = ParseHex16 (text, address);
    CHR (hr);

    result.m_address = address;
    out              = result;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseSpace
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugAddress::ParseSpace (char prefix, DebugAddress & out)
{
    HRESULT  hr    = S_OK;
    char     upper = (char) std::toupper ((unsigned char) prefix);
    Word     bank  = 0;



    if (upper == s_kSystemPrefix)
    {
        out.m_space = DebugAddressSpace::System;
    }
    else if (upper == s_kRomPrefix)
    {
        out.m_space = DebugAddressSpace::Rom;
    }
    else
    {
        hr = ParseHex16 (std::string_view (&prefix, 1), bank);
        CHR (hr);

        CBREx (bank <= kLastBank, s_kInvalidData);

        out.m_space = DebugAddressSpace::Bank;
        out.m_bank  = (Byte) bank;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseHex16
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugAddress::ParseHex16 (std::string_view text, Word & out)
{
    HRESULT   hr      = S_OK;
    uint32_t  value   = 0;
    size_t    digits  = 0;



    if (!text.empty() && text[0] == s_kHexPrefix)
    {
        text = text.substr (1);
    }

    digits = text.size();
    CBREx (digits >= 1 && digits <= s_kMaxHexDigits, s_kInvalidData);

    for (char c : text)
    {
        bool  isDigit = c >= '0' && c <= '9';
        bool  isUpper = c >= 'A' && c <= 'F';
        bool  isLower = c >= 'a' && c <= 'f';
        int   nibble  = isDigit ? c - '0' : isUpper ? c - 'A' + s_kDecimalDigits : c - 'a' + s_kDecimalDigits;



        CBREx (isDigit || isUpper || isLower, s_kInvalidData);

        value = (value << s_kHexDigitBits) | (uint32_t) nibble;
    }

    out = (Word) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Format
//
//  The text form, with the given separator after the space. The address is
//  always four uppercase hex digits.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugAddress::Format (char separator) const
{
    switch (m_space)
    {
        case DebugAddressSpace::Bank:    return std::format ("{:X}{}{:04X}", m_bank, separator, m_address);
        case DebugAddressSpace::System:  return std::format ("{}{}{:04X}", s_kSystemPrefix, separator, m_address);
        case DebugAddressSpace::Rom:     return std::format ("{}{}{:04X}", s_kRomPrefix, separator, m_address);

        default:
            break;
    }

    return std::format ("{:04X}", m_address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Validate
//
//  A bank is allowed only on an address in $2000-$9FFF, and only a bank the
//  machine has; bankCount is 0 on a machine without banks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugAddress::Validate (size_t bankCount) const
{
    HRESULT  hr         = S_OK;
    bool     isBanked   = IsInBankedRegion();
    bool     isFitted   = (size_t) m_bank < bankCount;



    if (m_space == DebugAddressSpace::Bank)
    {
        CBREx (isBanked, s_kInvalidData);
        CBREx (isFitted, s_kInvalidData);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatBankRule
//
//  The second line of the illegal address error.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugAddress::FormatBankRule (size_t bankCount)
{
    size_t  lastBank = (bankCount > 0) ? bankCount - 1 : 0;



    return std::format ("A bank applies only to addresses ${:04X}-${:04X}, and this machine has banks 0 through {}.", kFirstBanked, kLastBanked, lastBank);
}
