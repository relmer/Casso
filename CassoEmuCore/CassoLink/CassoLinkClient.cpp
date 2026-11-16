#include "Pch.h"

#include "CassoLink/CassoLinkClient.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::HasLowercase
//
//  $FBB3 is 6 on a //e, a //c and a IIgs, and $38 on a ][ and a ][+.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkClient::HasLowercase() const
{
    static constexpr Byte  kFamilyIIe = 0x06;



    return hasRomId && romId[0] == kFamilyIIe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::GetModelName
//
//  From the identification bytes Apple's technical notes list for each
//  model.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkClient::GetModelName() const
{
    Byte  family  = romId[0];
    Byte  version = romId[1];
    Byte  release = romId[2];



    if (!hasRomId)
    {
        return std::wstring();
    }

    if (family == 0x38)
    {
        return (romId[3] == 0xAD) ? L"Apple ][" : L"Apple ][+";
    }

    if (family != 0x06)
    {
        return L"Apple II";
    }

    if (version == 0xEA)
    {
        return L"Apple //e";
    }

    if (version == 0xE0)
    {
        return (romId[4] != 0) ? L"Apple IIgs" : L"Apple //e enhanced";
    }

    switch (release)
    {
        case 0xFF:  return L"Apple //c";
        case 0x00:  return L"Apple //c (ROM 0)";
        case 0x03:  return L"Apple //c (ROM 3)";
        case 0x04:  return L"Apple //c (ROM 4)";
        case 0x05:  return L"Apple //c Plus";
        default:    return L"Apple //c";
    }
}
