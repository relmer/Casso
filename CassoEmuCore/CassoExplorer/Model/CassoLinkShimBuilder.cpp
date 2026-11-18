#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "Core/TextEncoding.h"
#include "Seams/Win32NetworkAdapters.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkShimBuilder::MakeDialCommand
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkShimBuilder::MakeDialCommand (const std::wstring & pcName, const CassoLinkSettings & settings)
{
    size_t       colon = settings.address.rfind (':');
    std::string  port  = colon == std::string::npos ? settings.address : settings.address.substr (colon + 1);
    std::string  dial;



    if (settings.transport == LinkTransport::WifiModem)
    {
        dial = std::format ("ATDT {}.local:{}", TextEncoding::WideToNarrow (pcName), port);
    }

    return dial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkShimBuilder::MakeShimOptions
//
//  For a link with no address or one that cannot be used, the shim's
//  network is zeros; the link dialog accepts only an IPv4 address or none
//  for an Uthernet II.
//
////////////////////////////////////////////////////////////////////////////////

LinkShimOptions CassoLinkShimBuilder::MakeShimOptions (const std::wstring & pcName, const CassoLinkSettings & settings)
{
    HRESULT                      hr       = S_OK;
    LinkShimOptions              options;
    std::vector<NetworkAdapter>  adapters;



    options.baud = settings.baud;
    options.slot = (settings.transport == LinkTransport::Uthernet) ? kUthernetSlot : options.slot;
    options.dial = MakeDialCommand (pcName, settings);

    BAIL_OUT_IF (settings.transport != LinkTransport::Uthernet, S_OK);

    options.chip = LinkChip::W5100;

    //  With no adapters listed, the defaults apply.
    hr = Win32NetworkAdapters::List (adapters);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = MakeNetworkConfig (settings, adapters, options.network);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkShimBuilder::MakeNetworkConfig
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkShimBuilder::MakeNetworkConfig (const CassoLinkSettings & settings, const std::vector<NetworkAdapter> & adapters, LinkNetworkConfig & outConfig)
{
    constexpr std::array<Byte, 5>         kMacPrefix   = { 0x02, 'C', 'A', 'S', 'S' };   // locally administered, unicast
    constexpr Byte                        kGatewayHost = 1;
    HRESULT                               hr           = S_OK;
    size_t                                colon        = settings.address.rfind (':');
    std::string                           host         = settings.address.substr (0, colon);
    std::string                           port         = (colon == std::string::npos) ? std::string() : settings.address.substr (colon + 1);
    IN_ADDR                               parsed       = {};
    int                                   converted    = 0;
    unsigned                              number       = 0;
    std::from_chars_result                read         = {};
    bool                                  numeric      = false;
    bool                                  sameSubnet   = false;



    outConfig = LinkNetworkConfig();

    converted = inet_pton (AF_INET, host.c_str(), &parsed);
    CBREx (converted == 1, E_INVALIDARG);

    std::memcpy (outConfig.address.data(), &parsed, outConfig.address.size());

    if (!port.empty())
    {
        read    = std::from_chars (port.data(), port.data() + port.size(), number);
        numeric = read.ec == std::errc() && read.ptr == port.data() + port.size() && number >= 1 && number <= UINT16_MAX;
        CBREx (numeric, E_INVALIDARG);

        outConfig.port = (uint16_t) number;
    }

    for (const NetworkAdapter & adapter : adapters)
    {
        sameSubnet = true;

        for (size_t i = 0; i < outConfig.address.size(); i++)
        {
            sameSubnet = sameSubnet && (adapter.address[i] & adapter.mask[i]) == (outConfig.address[i] & adapter.mask[i]);
        }

        if (sameSubnet)
        {
            outConfig.mask    = adapter.mask;
            outConfig.gateway = adapter.gateway;
            break;
        }
    }

    if (!sameSubnet)
    {
        for (size_t i = 0; i < outConfig.address.size(); i++)
        {
            outConfig.gateway[i] = (Byte) (outConfig.address[i] & outConfig.mask[i]);
        }

        outConfig.gateway.back() |= kGatewayHost;
    }

    std::copy (kMacPrefix.begin(), kMacPrefix.end(), outConfig.mac.begin());
    outConfig.mac.back() = outConfig.address.back();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkShimBuilder::GetPcName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkShimBuilder::GetPcName()
{
    wchar_t  name[256] = {};
    DWORD    size      = (DWORD) std::size (name);



    if (!GetComputerNameExW (ComputerNameDnsHostname, name, &size))
    {
        size = 0;
    }

    return std::wstring (name, size);
}
