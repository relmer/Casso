#pragma once

#include "Pch.h"

#include "Seams/IHostSerialPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPorts
//
//  IHostSerialPorts over the PC's COM ports: the ports present are the
//  values of HKLM\HARDWARE\DEVICEMAP\SERIALCOMM, which the serial drivers
//  update as devices come and go, and a port opens as \\.\COMn.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostSerialPorts : public IHostSerialPorts
{
public:

    HRESULT  EnumeratePorts (std::vector<std::string> & outNames)                                 override;
    HRESULT  Open           (const std::string & name, std::unique_ptr<IHostSerialPort> & outPort) override;

    //  COM2 before COM10.
    static bool  IsBefore (const std::string & a, const std::string & b);
};
