#pragma once

#include "Pch.h"

#include "Seams/IHostSerialPort.h"
#include "FakeHostSerialPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostSerialPorts : public IHostSerialPorts
{
public:

    HRESULT  EnumeratePorts (std::vector<std::string> & outNames)                                 override;
    HRESULT  Open           (const std::string & name, std::unique_ptr<IHostSerialPort> & outPort) override;

    //  A port present on the PC; its state, for the test to drive.
    std::shared_ptr<FakeSerialPortState>  AddPort (const std::string & name);

private:

    std::vector<std::shared_ptr<FakeSerialPortState>>  m_ports;
};
