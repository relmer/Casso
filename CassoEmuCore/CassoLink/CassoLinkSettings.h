#pragma once

#include "Pch.h"

#include "Seams/ByteStreamSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSettings
//
//  One link, as CassoExplorerPrefs.json stores it. The port is a COM port's
//  name, or empty to search the ports for the Apple. The address is
//  host:port for the TCP transports; for Emulated it is Casso's own serial
//  port on 127.0.0.1, and for Uthernet the Apple's own address and the port
//  its shim listens on. The slot is the Apple's serial port or card: a //c's
//  port number, 2 for the link, or a Super Serial Card's or an Uthernet II's
//  slot.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkSettings
{
    std::string    name;
    LinkTransport  transport         = LinkTransport::Serial;
    std::string    port;
    std::string    address;
    DWORD          baud              = kDefaultBaud;
    DWORD          bootstrapBaud     = kDefaultBootstrapBaud;
    bool           hardwareHandshake = false;
    int            slot              = kDefaultSlot;

    static constexpr DWORD  kDefaultBaud          = 115200;
    static constexpr DWORD  kDefaultBootstrapBaud = 9600;
    static constexpr int    kDefaultSlot          = 2;

    //  The transport's name in the file, and back; false for a name that is
    //  none of them.
    static const char *  GetTransportName  (LinkTransport transport);
    static bool          TryParseTransport (const std::string & name, LinkTransport & outTransport);

    bool  operator== (const CassoLinkSettings & other) const = default;
};
