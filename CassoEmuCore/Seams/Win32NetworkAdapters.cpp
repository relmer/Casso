#include "Pch.h"

#include "Seams/Win32NetworkAdapters.h"

#pragma comment(lib, "iphlpapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkAdapters::List
//
//  Each unicast address's mask is from its prefix length; the gateway is the
//  adapter's first IPv4 one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32NetworkAdapters::List (std::vector<NetworkAdapter> & outAdapters)
{
    constexpr ULONG                       kFlags   = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    constexpr int                         kBits    = 32;
    constexpr int                         kByte    = 8;
    HRESULT                               hr       = S_OK;
    ULONG                                 size     = 0;
    ULONG                                 result   = 0;
    std::vector<Byte>                     buffer;
    IP_ADAPTER_ADDRESSES                * adapter  = nullptr;
    IP_ADAPTER_UNICAST_ADDRESS          * unicast  = nullptr;
    IP_ADAPTER_GATEWAY_ADDRESS_LH       * gateway  = nullptr;
    const sockaddr_in                   * ipv4     = nullptr;
    NetworkAdapter                        entry;
    uint32_t                              address  = 0;
    uint32_t                              mask     = 0;
    uint32_t                              router   = 0;
    bool                                  up       = false;
    int                                   shift    = 0;



    outAdapters.clear();

    result = GetAdaptersAddresses (AF_INET, kFlags, nullptr, nullptr, &size);
    CBREx (result == ERROR_BUFFER_OVERFLOW, HRESULT_FROM_WIN32 (result));

    buffer.resize (size);

    result = GetAdaptersAddresses (AF_INET, kFlags, nullptr, (IP_ADAPTER_ADDRESSES *) buffer.data(), &size);
    CBREx (result == ERROR_SUCCESS, HRESULT_FROM_WIN32 (result));

    for (adapter = (IP_ADAPTER_ADDRESSES *) buffer.data(); adapter != nullptr; adapter = adapter->Next)
    {
        up = adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK;

        if (!up)
        {
            continue;
        }

        router = 0;

        for (gateway = adapter->FirstGatewayAddress; gateway != nullptr && router == 0; gateway = gateway->Next)
        {
            if (gateway->Address.lpSockaddr->sa_family == AF_INET)
            {
                ipv4   = (const sockaddr_in *) gateway->Address.lpSockaddr;
                router = ntohl (ipv4->sin_addr.s_addr);
            }
        }

        for (unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
        {
            if (unicast->Address.lpSockaddr->sa_family != AF_INET)
            {
                continue;
            }

            ipv4    = (const sockaddr_in *) unicast->Address.lpSockaddr;
            address = ntohl (ipv4->sin_addr.s_addr);
            mask    = (unicast->OnLinkPrefixLength == 0) ? 0 : (uint32_t) (~0ull << (kBits - unicast->OnLinkPrefixLength));

            for (size_t i = 0; i < entry.address.size(); i++)
            {
                shift = kBits - kByte - (int) i * kByte;

                entry.address[i] = (Byte) (address >> shift);
                entry.mask[i]    = (Byte) (mask >> shift);
                entry.gateway[i] = (Byte) (router >> shift);
            }

            outAdapters.push_back (entry);
        }
    }

Error:
    return hr;
}
