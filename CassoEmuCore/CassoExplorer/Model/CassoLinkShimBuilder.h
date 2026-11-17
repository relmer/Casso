#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSettings.h"
#include "CassoLink/LinkProgram.h"

struct NetworkAdapter;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkShimBuilder
//
//  What is written into the shim on a connection's boot disk.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkShimBuilder
{
public:
    //  The dial command itself, as the shim on a boot disk sends it to the
    //  modem; empty for any transport but the Wi-Fi modem.
    static std::string      MakeDialCommand   (const std::wstring & pcName, const CassoLinkSettings & settings);

    //  What is written into the shim on a link's boot disk: the link's speed,
    //  slot and dial command, or for an Uthernet II, the W5100 build, with the
    //  network for the link's address and the PC's adapters.
    static LinkShimOptions  MakeShimOptions   (const std::wstring & pcName, const CassoLinkSettings & settings);

    //  An Uthernet II's network for a link's address: the address and port;
    //  the mask and gateway of the PC's adapter on its subnet, or
    //  255.255.255.0 and the subnet's .1; and a locally administered MAC
    //  address ending in the address's last byte. Fails for a host that is
    //  not an IPv4 address.
    static HRESULT          MakeNetworkConfig (const CassoLinkSettings & settings, const std::vector<NetworkAdapter> & adapters, LinkNetworkConfig & outConfig);

    //  This PC's DNS host name.
    static std::wstring     GetPcName         ();

    //  The slot the Uthernet II ships in, which a W5100 shim is built for.
    static constexpr int    kUthernetSlot = 3;
};