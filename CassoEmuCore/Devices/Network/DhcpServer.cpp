#include "Pch.h"

#include "Devices/Network/DhcpServer.h"
#include "Devices/Network/PacketCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer::Process
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> DhcpServer::Process (std::span<const Byte> request) const
{
    Byte  messageType = 0;



    if (request.size() < kOptionsOffset
        || request[kOpOffset] != kBootRequest
        || request[kHtypeOffset] != kHtypeEthernet
        || request[kHlenOffset] != kHlenEthernet
        || PacketCodec::ReadU32 (request, kCookieOffset) != kMagicCookie)
    {
        return {};
    }

    messageType = FindMessageType (request);

    if (messageType == kMessageDiscover)
    {
        return BuildReply (request, kMessageOffer);
    }

    if (messageType == kMessageRequest)
    {
        return BuildReply (request, kMessageAck);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer::FindMessageType
//
//  Option 53, or 0 when the options end without it or run off the packet.
//
////////////////////////////////////////////////////////////////////////////////

Byte DhcpServer::FindMessageType (std::span<const Byte> request)
{
    size_t  i = kOptionsOffset;



    while (i < request.size() && request[i] != kOptionEnd)
    {
        if (request[i] == kOptionPad)
        {
            i++;
            continue;
        }

        if (i + 1 >= request.size())
        {
            break;
        }

        if (request[i] == kOptionMessageType && request[i + 1] == 1 && i + 2 < request.size())
        {
            return request[i + 2];
        }

        i += 2 + (size_t) request[i + 1];
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer::AppendOption (one byte)
//
////////////////////////////////////////////////////////////////////////////////

void DhcpServer::AppendOption (
    std::vector<Byte>  & reply,
    Byte                 code,
    Byte                 value)
{
    reply.push_back (code);
    reply.push_back (1);
    reply.push_back (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer::AppendOption (four bytes)
//
////////////////////////////////////////////////////////////////////////////////

void DhcpServer::AppendOption (
    std::vector<Byte>  & reply,
    Byte                 code,
    uint32_t             value)
{
    constexpr Byte  kWidth = sizeof (uint32_t);
    size_t          at     = 0;



    reply.push_back (code);
    reply.push_back (kWidth);

    at = reply.size();
    reply.resize (at + kWidth);
    PacketCodec::WriteU32 (reply, at, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer::BuildReply
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> DhcpServer::BuildReply (
    std::span<const Byte>   request,
    Byte                    messageType) const
{
    std::vector<Byte>  reply (kOptionsOffset, 0);



    reply[kOpOffset]    = kBootReply;
    reply[kHtypeOffset] = kHtypeEthernet;
    reply[kHlenOffset]  = kHlenEthernet;

    std::copy_n (request.begin() + kXidOffset,    kXidLength,    reply.begin() + kXidOffset);
    std::copy_n (request.begin() + kFlagsOffset,  kFlagsLength,  reply.begin() + kFlagsOffset);
    std::copy_n (request.begin() + kChaddrOffset, kChaddrLength, reply.begin() + kChaddrOffset);

    PacketCodec::WriteU32 (reply, kYiaddrOffset, m_lease.clientIp);
    PacketCodec::WriteU32 (reply, kSiaddrOffset, m_lease.router);
    PacketCodec::WriteU32 (reply, kCookieOffset, kMagicCookie);

    AppendOption (reply, kOptionMessageType, messageType);
    AppendOption (reply, kOptionServerId,    m_lease.router);
    AppendOption (reply, kOptionLeaseTime,   kLeaseSeconds);
    AppendOption (reply, kOptionSubnetMask,  m_lease.subnetMask);
    AppendOption (reply, kOptionRouter,      m_lease.router);
    AppendOption (reply, kOptionDns,         m_lease.dnsServer);
    reply.push_back (kOptionEnd);

    if (reply.size() < kMinimumReply)
    {
        reply.resize (kMinimumReply, kOptionPad);
    }

    return reply;
}