#include "Pch.h"

#include "AppleLink/AppleLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSettings::GetTransportName
//
////////////////////////////////////////////////////////////////////////////////

const char * AppleLinkSettings::GetTransportName (LinkTransport transport)
{
    const char *  name = "Serial";



    switch (transport)
    {
        case LinkTransport::Tcp:       name = "Tcp";       break;
        case LinkTransport::WifiModem: name = "WifiModem"; break;
        case LinkTransport::Emulated:  name = "Emulated";  break;
        default:                       name = "Serial";    break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSettings::TryParseTransport
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkSettings::TryParseTransport (const std::string & name, LinkTransport & outTransport)
{
    bool  found = false;



    for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::WifiModem, LinkTransport::Emulated })
    {
        if (!found && name == GetTransportName (transport))
        {
            outTransport = transport;
            found        = true;
        }
    }

    return found;
}
