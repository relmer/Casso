#include "Pch.h"

#include "FakeHostSerialPorts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPorts::AddPort
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FakeSerialPortState> FakeHostSerialPorts::AddPort (const std::string & name)
{
    std::shared_ptr<FakeSerialPortState>  state = std::make_shared<FakeSerialPortState>();



    state->name = name;
    m_ports.push_back (state);

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPorts::EnumeratePorts
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPorts::EnumeratePorts (std::vector<std::string> & outNames)
{
    outNames.clear();

    for (const auto & state : m_ports)
    {
        if (state->isPresent && !state->isUnplugged)
        {
            outNames.push_back (state->name);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPorts::Open
//
//  A port open already, here or in another program, is in use, as Windows
//  opens a COM port for one handle at a time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSerialPorts::Open (
    const std::string                 & name,
    std::unique_ptr<IHostSerialPort>  & outPort)
{
    HRESULT  hr      = S_OK;
    auto     found   = std::find_if (m_ports.begin(), m_ports.end(), [&name] (const auto & state) { return state->name == name; });
    bool     isHere  = found != m_ports.end() && (*found)->isPresent && !(*found)->isUnplugged;
    bool     isTaken = isHere && ((*found)->isInUse || (*found)->isOpen);



    CBREx (isHere,   HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    CBREx (!isTaken, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    outPort = std::make_unique<FakeHostSerialPort> (*found);

Error:
    return hr;
}
