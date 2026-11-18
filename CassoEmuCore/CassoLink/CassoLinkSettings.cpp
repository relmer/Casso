#include "Pch.h"

#include "CassoLink/CassoLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::GetTransportName
//
////////////////////////////////////////////////////////////////////////////////

const char * CassoLinkSettings::GetTransportName (LinkTransport transport)
{
    const char *  name = "Serial";



    switch (transport)
    {
        case LinkTransport::Tcp:          name = "Tcp";          break;
        case LinkTransport::WifiModem:    name = "WifiModem";    break;
        case LinkTransport::Emulated:     name = "Emulated";     break;
        case LinkTransport::Uthernet:     name = "Uthernet";     break;
        case LinkTransport::AdtProServer: name = "AdtProServer"; break;
        case LinkTransport::Cassette:     name = "Cassette";     break;
        case LinkTransport::Udp:          name = "Udp";          break;
        default:                          name = "Serial";       break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::TryParseTransport
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSettings::TryParseTransport (const std::string & name, LinkTransport & outTransport)
{
    bool  found = false;



    for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::WifiModem, LinkTransport::Emulated, LinkTransport::Uthernet, LinkTransport::AdtProServer, LinkTransport::Cassette, LinkTransport::Udp })
    {
        if (!found && name == GetTransportName (transport))
        {
            outTransport = transport;
            found        = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::MakeName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkSettings::MakeName (const CassoLinkSettings & settings)
{
    size_t       colon = settings.address.rfind (':');
    std::string  port  = (colon == std::string::npos) ? std::string() : settings.address.substr (colon + 1);
    std::string  name;



    switch (settings.transport)
    {
        case LinkTransport::Serial:       name = settings.port.empty() ? std::string ("Serial (any port)") : settings.port;               break;
        case LinkTransport::WifiModem:    name = std::format ("Wi-Fi modem on port {}", kModemPort);                                     break;
        case LinkTransport::Emulated:     name = std::format ("Casso on port {}", port);                                                 break;
        case LinkTransport::Uthernet:     name = settings.address.empty() ? std::string ("Uthernet II (any address)")
                                                                          : std::format ("Uthernet II at {}", settings.address);        break;
        case LinkTransport::AdtProServer: name = settings.IsComPort() ? std::format ("ADTPro server on {}", settings.port)
                                                                      : std::format ("ADTPro server at {}", settings.address);          break;
        case LinkTransport::Cassette:     name = "Cassette ports (audio)";                                                               break;
        case LinkTransport::Udp:          name = settings.address.empty() ? std::string ("Uthernet or LANceGS (any address)")
                                                                          : std::format ("Uthernet or LANceGS at {}", settings.address); break;
        default:                          name = std::format ("TCP {}", settings.address);                                              break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::IsComPort
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSettings::IsComPort() const
{
    return transport == LinkTransport::Serial || (transport == LinkTransport::AdtProServer && !port.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::IsSearch
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSettings::IsSearch() const
{
    return (transport == LinkTransport::Uthernet || transport == LinkTransport::Udp) && address.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::CanBootstrap
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSettings::CanBootstrap() const
{
    return transport != LinkTransport::Uthernet && transport != LinkTransport::Udp && transport != LinkTransport::AdtProServer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings::IsSavable
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSettings::IsSavable (LinkTransport transport)
{
    return transport != LinkTransport::Emulated;
}
