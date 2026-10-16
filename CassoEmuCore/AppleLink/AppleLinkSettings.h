#pragma once

#include "Pch.h"

#include "Seams/ByteStreamSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSettings
//
//  One link, as CassoExplorerPrefs.json stores it. The port is a COM port's
//  name, or empty to search the ports for the Apple. The address is
//  host:port for the TCP transports; for Emulated it is Casso's own serial
//  port on 127.0.0.1.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleLinkSettings
{
    std::string    name;
    LinkTransport  transport         = LinkTransport::Serial;
    std::string    port;
    std::string    address;
    DWORD          baud              = kDefaultBaud;
    DWORD          bootstrapBaud     = kDefaultBootstrapBaud;
    bool           hardwareHandshake = false;

    static constexpr DWORD  kDefaultBaud          = 115200;
    static constexpr DWORD  kDefaultBootstrapBaud = 9600;

    //  The transport's name in the file, and back; false for a name that is
    //  none of them.
    static const char *  GetTransportName  (LinkTransport transport);
    static bool          TryParseTransport (const std::string & name, LinkTransport & outTransport);

    bool  operator== (const AppleLinkSettings & other) const = default;
};
