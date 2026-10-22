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
        case LinkTransport::Tcp:       name = "Tcp";       break;
        case LinkTransport::WifiModem: name = "WifiModem"; break;
        case LinkTransport::Emulated:  name = "Emulated";  break;
        case LinkTransport::Uthernet:  name = "Uthernet";  break;
        default:                       name = "Serial";    break;
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



    for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::WifiModem, LinkTransport::Emulated, LinkTransport::Uthernet })
    {
        if (!found && name == GetTransportName (transport))
        {
            outTransport = transport;
            found        = true;
        }
    }

    return found;
}
