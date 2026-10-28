#pragma once

#include "Pch.h"

#include "Devices/Serial/SerialLineFormat.h"
#include "Seams/IHostSerialPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeSerialPortState
//
//  Everything about one fake PC serial port, shared between the
//  IHostSerialPort the code under test owns and the test, which is the
//  device at the far end: it sets the bytes and errors that arrive and the
//  status lines, and reads what the port was configured and sent.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeSerialPortState
{
    std::string                    name;
    bool                           isPresent   = true;
    bool                           isInUse     = false;    // another program has it open
    bool                           isUnplugged = false;    // every call fails
    bool                           isOpen      = false;
    int                            opens       = 0;

    std::vector<uint32_t>          rates       = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200 };
    bool                           isAnyRate   = true;
    std::vector<SerialLineFormat>  formats;               // each Configure, in order
    bool                           dtr         = false;
    bool                           rts         = false;
    bool                           isBreak     = false;
    std::vector<Byte>              written;

    std::deque<Byte>               inbound;
    SerialPortErrors               errors;                // returned by the next Read, then cleared
    SerialPortLines                lines       = { true, true, true };
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSerialPort
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostSerialPort : public IHostSerialPort
{
public:

    explicit FakeHostSerialPort (std::shared_ptr<FakeSerialPortState> state);
    ~FakeHostSerialPort () override;

    HRESULT  GetBaudRates (std::vector<uint32_t> & outRates, bool & outIsAnyRate)                  override;
    HRESULT  Configure    (const SerialLineFormat & format)                                        override;
    HRESULT  SetControl   (bool dtr, bool rts)                                                     override;
    HRESULT  SetBreak     (bool isOn)                                                              override;
    HRESULT  Write        (std::span<const Byte> bytes, size_t & outWritten)                       override;
    HRESULT  Read         (std::span<Byte> buffer, size_t & outRead, SerialPortErrors & outErrors) override;
    HRESULT  GetLines     (SerialPortLines & outLines)                                             override;

private:

    HRESULT  CheckPlugged () const;

    std::shared_ptr<FakeSerialPortState>  m_state;
};
