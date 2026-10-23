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
//  its shim listens on. A Wi-Fi modem listens on kModemPort. An ADTPro
//  server is on its COM port, or with none on its address, and serves the
//  folder. The Apple's port or slot is not set: the Apple side finds it.
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
    std::wstring   folder;

    static constexpr DWORD     kDefaultBaud          = 115200;
    static constexpr DWORD     kDefaultBootstrapBaud = 9600;
    static constexpr uint16_t  kModemPort            = 6400;

    //  The transport's name in the file, and back; false for a name that is
    //  none of them.
    static const char *  GetTransportName  (LinkTransport transport);
    static bool          TryParseTransport (const std::string & name, LinkTransport & outTransport);

    //  The connection's name, made from its settings (FR-083c): "Serial (any
    //  port)" or the COM port, "TCP" and the address, "Wi-Fi modem on port
    //  6400", "Uthernet II at" and the address, "ADTPro server on" the COM
    //  port or "at" the address, and for a running Casso, "Casso on port" and
    //  its TCP port. Two connections with the same settings have the same
    //  name.
    static std::string   MakeName          (const CassoLinkSettings & settings);

    //  Whether the connection is over a COM port: a Serial one, or an ADTPro
    //  server with a COM port.
    bool                 IsComPort         () const;

    //  Whether a user can save the connection: every transport but a running
    //  Casso's, which is listed only while that Casso runs.
    static bool          IsSavable         (LinkTransport transport);

    bool  operator== (const CassoLinkSettings & other) const = default;
};
