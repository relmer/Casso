#include "Pch.h"

#include "CassoLink/CassoLinkClient.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::HasLowercase
//
//  $FBB3 is 6 on a //e, a //c and a IIgs, and $38 on a ][ and a ][+. The
//  Apple ///'s console has small letters.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkClient::HasLowercase() const
{
    static constexpr Byte  kFamilyIIe = 0x06;



    return isSos || (hasRomId && romId[0] == kFamilyIIe);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::GetModelName
//
//  From the identification bytes Apple's technical notes list for each
//  model. The SOS program sends zeros in their place.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkClient::GetModelName() const
{
    Byte  family  = romId[0];
    Byte  version = romId[1];
    Byte  release = romId[2];



    if (isSos)
    {
        return L"Apple ///";
    }

    if (!hasRomId)
    {
        return std::wstring();
    }

    if (family == 0x38)
    {
        return L"Apple ][";
    }

    // $FBB3 is $EA on a ][+, which has $AD at $FB1E as the ][ does.
    if (family == 0xEA)
    {
        return L"Apple ][+";
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





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient::GetSlotCards
//
//  By the signature bytes ProDOS and Pascal check: $20 $00 $03 is a block
//  device's firmware, whose $Cn07 is $3C on a Disk II and $00 on a
//  SmartPort controller, and $38 $18 at $Cn05 and $Cn07 is Pascal 1.1
//  firmware, as a serial or parallel card has. Any other bytes are an empty
//  slot or a card with neither.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkClient::GetSlotCards() const
{
    std::string  cards;



    if (!hasSlotIds)
    {
        return cards;
    }

    for (size_t slot = 0; slot < 7; slot++)
    {
        const Byte  * id   = &slotIds[slot * 4];
        const char  * card = nullptr;

        if (id[0] == 0x20 && id[1] == 0x00 && id[2] == 0x03)
        {
            card = (id[3] == 0x3C) ? "Disk II" : (id[3] == 0x00) ? "SmartPort" : "block device";
        }
        else if (id[2] == 0x38 && id[3] == 0x18)
        {
            card = "Pascal firmware card";
        }

        if (card != nullptr)
        {
            cards += std::format ("{}{}: {}", cards.empty() ? "" : ", ", slot + 1, card);
        }
    }

    return cards;
}
