#pragma once

#include "Pch.h"

#include "Config/MacAddress.h"

class JsonValue;





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwardProtocol
//
////////////////////////////////////////////////////////////////////////////////

enum class PortForwardProtocol
{
    Tcp,
    Udp,
};





////////////////////////////////////////////////////////////////////////////////
//
//  PortForward
//
//  Connections (TCP) or datagrams (UDP) arriving at `hostPort` on the PC go
//  to `guestPort` on the emulated Apple II.
//
////////////////////////////////////////////////////////////////////////////////

struct PortForward
{
    PortForwardProtocol  protocol  = PortForwardProtocol::Tcp;
    uint16_t             hostPort  = 0;
    uint16_t             guestPort = 0;

    bool operator== (const PortForward & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings
//
//  The Uthernet II's settings, stored as the `network` object on the card's
//  slot entry. The MAC address is the card's label value for the user to
//  enter in guest software; the chip itself starts with no MAC.
//
////////////////////////////////////////////////////////////////////////////////

class NetworkCardSettings
{
public:

    static constexpr const char *  kpszJsonKey = "network";

    MacAddress                mac;
    bool                      lanAccess    = false;
    std::vector<PortForward>  portForwards;

    bool operator== (const NetworkCardSettings & other) const = default;

    // Reads the `network` object. Invalid entries take their defaults and
    // add a line to `outWarnings`; the read itself fails only on a value of
    // the wrong JSON type for the whole object.
    static HRESULT  FromJson       (const JsonValue          & object,
                                    NetworkCardSettings      & outSettings,
                                    std::vector<std::string> & outWarnings);
    JsonValue       ToJson         () const;

    // Empty when valid; otherwise the problem, as a sentence.
    static std::string  ValidatePortForward (const PortForward              & forward,
                                             const std::vector<PortForward> & existing);

    static const char *  ProtocolToToken (PortForwardProtocol protocol);
    static bool          TryParseProtocol (std::string_view token, PortForwardProtocol & outProtocol);

private:

    static bool  TryReadPortForward (const JsonValue & entry, PortForward & outForward);
};
