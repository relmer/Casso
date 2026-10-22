#include "Pch.h"

#include "CassoLink/LinkAnnouncement.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAnnouncement::Parse
//
////////////////////////////////////////////////////////////////////////////////

bool LinkAnnouncement::Parse (std::span<const Byte> datagram, LinkAnnouncement & outAnnouncement)
{
    static constexpr std::string_view  s_kTag = "CASSOLINK";
    bool                               valid  = datagram.size() >= kLength
                                             && std::equal (s_kTag.begin(), s_kTag.end(), datagram.begin())
                                             && datagram[s_kTag.size()] == kVersion;
    size_t                             at     = s_kTag.size() + 1;



    if (valid)
    {
        std::copy_n (datagram.begin() + (ptrdiff_t) at, 4, outAnnouncement.address.begin());
        at += 4;

        outAnnouncement.port = (uint16_t) ((datagram[at] << 8) | datagram[at + 1]);
        at += 2;

        std::copy_n (datagram.begin() + (ptrdiff_t) at, 6, outAnnouncement.mac.begin());
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAnnouncement::GetAddressText
//
////////////////////////////////////////////////////////////////////////////////

std::string LinkAnnouncement::GetAddressText() const
{
    return std::format ("{}.{}.{}.{}:{}", address[0], address[1], address[2], address[3], port);
}
