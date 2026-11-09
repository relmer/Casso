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

//  Emulated is a running Casso's connection, listed only while it runs and
//  never saved; every other transport is one a user adds.
enum class LinkTransport
{
    Serial,
    Tcp,
    WifiModem,
    Emulated,
    Uthernet,
    AdtProServer,
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

    //  A COM port's write that did not finish within its timeout: with the
    //  handshake on and CTS off, the cable may not connect CTS, and
    //  otherwise the line timed out.
    static constexpr HRESULT  kCtsOff = HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
    static HRESULT       ClassifyWriteStall (bool hardwareHandshake, bool ctsOn);
};
