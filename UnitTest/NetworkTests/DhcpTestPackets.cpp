#include "Pch.h"

#include "DhcpTestPackets.h"

#include "Devices/Network/PacketCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets::MakeRequest
//
//  op 1, Ethernet hardware, the xid and chaddr given, the magic cookie, and
//  option 53 with `messageType`, then the end option.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> DhcpTestPackets::MakeRequest (
    Byte                        messageType,
    const MacAddress::Octets  & mac,
    uint32_t                    xid)
{
    std::vector<Byte>  packet (240, 0);



    packet[0] = 1;
    packet[1] = 1;
    packet[2] = 6;
    PacketCodec::WriteU32 (packet, 4, xid);
    std::copy (mac.begin(), mac.end(), packet.begin() + 28);
    PacketCodec::WriteU32 (packet, 236, 0x63825363);

    packet.push_back (53);
    packet.push_back (1);
    packet.push_back (messageType);
    packet.push_back (255);

    return packet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets::GetOption32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DhcpTestPackets::GetOption32 (
    std::span<const Byte>   reply,
    Byte                    code)
{
    size_t  i = 240;



    while (i + 1 < reply.size() && reply[i] != 255)
    {
        if (reply[i] == 0)
        {
            i++;
            continue;
        }

        if (reply[i] == code && reply[i + 1] == 4)
        {
            return PacketCodec::ReadU32 (reply, i + 2);
        }

        i += 2 + reply[i + 1];
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets::GetMessageType
//
////////////////////////////////////////////////////////////////////////////////

Byte DhcpTestPackets::GetMessageType (std::span<const Byte> reply)
{
    size_t  i = 240;



    while (i + 2 < reply.size() && reply[i] != 255)
    {
        if (reply[i] == 53)
        {
            return reply[i + 2];
        }

        i += (reply[i] == 0) ? 1 : 2 + reply[i + 1];
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets::GetYourAddress
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DhcpTestPackets::GetYourAddress (std::span<const Byte> reply)
{
    return PacketCodec::ReadU32 (reply, 16);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets::GetXid
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DhcpTestPackets::GetXid (std::span<const Byte> reply)
{
    return PacketCodec::ReadU32 (reply, 4);
}
