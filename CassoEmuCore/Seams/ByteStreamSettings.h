#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ByteStreamSettings
//
//  The decisions the Win32 streams make, as pure functions so they are tested
//  without a port or a socket: a COM port's settings from a speed and a
//  handshake, and the address a TCP port binds to for a transport.
//
////////////////////////////////////////////////////////////////////////////////

enum class LinkTransport
{
    Serial,
    Tcp,
    WifiModem,
    Emulated,
    Uthernet,
};



struct ComSettings
{
    DWORD  baud              = 115200;
    Byte   dataBits          = 8;
    bool   hardwareHandshake = false;
};



class ByteStreamSettings
{
public:
    static constexpr const char *  kLoopbackAddress = "127.0.0.1";
    static constexpr const char *  kAnyAddress      = "0.0.0.0";

    //  Eight data bits, no parity and one stop bit, as ADTPro and the //c's
    //  firmware use; RTS and CTS when the handshake is on, otherwise both
    //  lines asserted constantly so a cable that wires them does no harm.
    static void          ToDcb          (const ComSettings & settings, DCB & outDcb);

    //  Only the Wi-Fi modem connects from another machine; every other TCP
    //  link is between programs on this PC.
    static const char *  GetBindAddress (LinkTransport transport);
};
