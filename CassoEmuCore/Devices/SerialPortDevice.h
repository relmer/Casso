#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortDevice
//
//  What is attached to an emulated serial port, read from the port's `device`
//  in the machine's configuration:
//
//      ""                  nothing; the port loops back on itself
//      "tcp:6502"          a TCP port on this PC that a program connects to
//      "com:COM3"          a PC COM port, at 115,200 baud
//      "com:COM3@9600"     a PC COM port at the speed given
//
////////////////////////////////////////////////////////////////////////////////

struct SerialPortDevice
{
    enum class Kind { Loopback, Tcp, Com };

    static constexpr DWORD  kDefaultBaud = 115200;

    Kind          kind    = Kind::Loopback;
    uint16_t      tcpPort = 0;
    std::wstring  comPort;
    DWORD         baud    = kDefaultBaud;

    static HRESULT      Parse  (const std::string & device, SerialPortDevice & outDevice);
    static std::string  Format (const SerialPortDevice & device);

private:
    static HRESULT      ParseNumber (std::string_view text, uint32_t maximum, uint32_t & outValue);

    static constexpr std::string_view  s_kTcpPrefix = "tcp:";
    static constexpr std::string_view  s_kComPrefix = "com:";
};
