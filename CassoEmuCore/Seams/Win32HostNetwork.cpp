#include "Pch.h"

#include "Seams/Win32HostNetwork.h"
#include "Seams/Win32HostNameLookup.h"
#include "Seams/Win32HostSocket.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork::Win32HostNetwork
//
//  After a failed start every CreateSocket fails, so the card works as if the
//  PC had no network, and the emulator does not stop.
//
////////////////////////////////////////////////////////////////////////////////

Win32HostNetwork::Win32HostNetwork()
{
    constexpr WORD  kWinsockVersion = MAKEWORD (2, 2);
    WSADATA         data            = {};
    int             result          = 0;



    result    = WSAStartup (kWinsockVersion, &data);
    m_started = (result == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork::~Win32HostNetwork
//
////////////////////////////////////////////////////////////////////////////////

Win32HostNetwork::~Win32HostNetwork()
{
    if (m_started)
    {
        WSACleanup();
        m_started = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork::CreateSocket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostNetwork::CreateSocket (
    HostSocketKind                  kind,
    std::unique_ptr<IHostSocket>  & outSocket)
{
    HRESULT  hr = S_OK;



    CBREx (m_started, E_NOT_VALID_STATE);

    hr = Win32HostSocket::Create (kind, outSocket);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork::GetDnsServer
//
//  The first IPv4 server in the PC's DNS list, from IP Helper. An IPv6-only
//  list, or none at all, is E_NOT_SET.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostNetwork::GetDnsServer (uint32_t & outIpv4)
{
    HRESULT                 hr        = S_OK;
    ULONG                   size      = 0;
    DWORD                   result    = 0;
    std::vector<Byte>       buffer;
    FIXED_INFO            * info      = nullptr;
    const IP_ADDR_STRING  * entry     = nullptr;
    IN_ADDR                 address   = {};
    int                     converted = 0;



    outIpv4 = 0;

    result = GetNetworkParams (nullptr, &size);
    CBREx (result == ERROR_BUFFER_OVERFLOW, HRESULT_FROM_WIN32 (result));

    buffer.resize (size);
    info = (FIXED_INFO *) buffer.data();

    result = GetNetworkParams (info, &size);
    CBREx (result == ERROR_SUCCESS, HRESULT_FROM_WIN32 (result));

    for (entry = &info->DnsServerList; entry != nullptr; entry = entry->Next)
    {
        converted = inet_pton (AF_INET, entry->IpAddress.String, &address);

        if (converted == 1 && address.s_addr != 0)
        {
            outIpv4 = ntohl (address.s_addr);
            break;
        }
    }

    CBREx (outIpv4 != 0, E_NOT_SET);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork::LookUpHost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostNetwork::LookUpHost (
    const std::string                 & host,
    std::unique_ptr<IHostNameLookup>  & outLookup)
{
    HRESULT                               hr     = S_OK;
    std::unique_ptr<Win32HostNameLookup>  lookup;



    CBREx (m_started, E_NOT_VALID_STATE);

    lookup = std::make_unique<Win32HostNameLookup>();
    CPRA (lookup);

    hr = lookup->Start (host);
    CHR (hr);

    outLookup = std::move (lookup);

Error:
    return hr;
}