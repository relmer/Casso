#pragma once

#include "Pch.h"

struct SerialLineFormat;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortErrors, SerialPortLines
//
//  The receive errors a PC serial port latched since the last read, and the
//  levels of its modem status inputs.
//
////////////////////////////////////////////////////////////////////////////////

struct SerialPortErrors
{
    bool  isFraming = false;
    bool  isParity  = false;
    bool  isOverrun = false;
    bool  isBreak   = false;
};

struct SerialPortLines
{
    bool  cts = false;
    bool  dsr = false;
    bool  dcd = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostSerialPort
//
//  One open serial port on the PC. No call waits: Read returns what has
//  arrived, and Write puts its bytes in the driver's buffer. A call that
//  fails after the port opened means the device is gone, as a USB adapter
//  unplugged. Destroying the object closes the port.
//
////////////////////////////////////////////////////////////////////////////////

class IHostSerialPort
{
public:

    virtual ~IHostSerialPort () = default;

    //  The rates the port can run at; outIsAnyRate when it takes any rate.
    virtual HRESULT  GetBaudRates (std::vector<uint32_t> & outRates, bool & outIsAnyRate)            = 0;

    //  The format's baud is a rate the port can run at.
    virtual HRESULT  Configure    (const SerialLineFormat & format)                                  = 0;
    virtual HRESULT  SetControl   (bool dtr, bool rts)                                               = 0;
    virtual HRESULT  SetBreak     (bool isOn)                                                        = 0;
    virtual HRESULT  Write        (std::span<const Byte> bytes, size_t & outWritten)                 = 0;
    virtual HRESULT  Read         (std::span<Byte> buffer, size_t & outRead, SerialPortErrors & outErrors) = 0;
    virtual HRESULT  GetLines     (SerialPortLines & outLines)                                       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostSerialPorts
//
//  The PC's serial ports. Unit tests substitute a fake in which the test is
//  the device at the far end of each port.
//
////////////////////////////////////////////////////////////////////////////////

class IHostSerialPorts
{
public:

    virtual ~IHostSerialPorts () = default;

    //  The ports present now, as "COM1", "COM3".
    virtual HRESULT  EnumeratePorts (std::vector<std::string> & outNames)                                 = 0;

    //  HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) for a port that is not
    //  present, and HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED) for one another
    //  program has open.
    virtual HRESULT  Open           (const std::string & name, std::unique_ptr<IHostSerialPort> & outPort) = 0;
};
