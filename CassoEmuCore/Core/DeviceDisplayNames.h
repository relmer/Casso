#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceDisplayNames
//
//  The display name of a device type, for the settings' hardware list and
//  the debugger's slot labels: "super-serial-card" is "Super Serial Card".
//
////////////////////////////////////////////////////////////////////////////////

class DeviceDisplayNames
{
public:
    static constexpr const char *  kpszNetworkCardDevice = "uthernet2";
    static constexpr const char *  kpszNetworkCardName   = "Uthernet II (Ethernet)";

    //  The type itself for a type with no display name.
    static std::string  Get (const std::string & type);
};
