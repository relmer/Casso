#pragma once

#include "Pch.h"

#include "Seams/IHostSerialPort.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSerialPort
//
//  IHostSerialPort over a COM port handle. Reads return at once: the read
//  timeouts are set so ReadFile takes only what has arrived, which
//  ClearCommError measures, along with the errors latched since the last
//  call. Hardware and software flow control in the driver are off; DTR and
//  RTS are set through EscapeCommFunction and kept across SetCommState.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostSerialPort : public IHostSerialPort
{
public:

    explicit Win32HostSerialPort (HANDLE handle) : m_handle (handle) {}
    ~Win32HostSerialPort () override;

    Win32HostSerialPort (const Win32HostSerialPort &)             = delete;
    Win32HostSerialPort & operator= (const Win32HostSerialPort &) = delete;

    //  Sets the buffers and the timeouts on a handle just opened.
    static HRESULT  Prepare (HANDLE handle);

    HRESULT  GetBaudRates (std::vector<uint32_t> & outRates, bool & outIsAnyRate)                  override;
    HRESULT  Configure    (const SerialLineFormat & format)                                        override;
    HRESULT  SetControl   (bool dtr, bool rts)                                                     override;
    HRESULT  SetBreak     (bool isOn)                                                              override;
    HRESULT  Write        (std::span<const Byte> bytes, size_t & outWritten)                       override;
    HRESULT  Read         (std::span<Byte> buffer, size_t & outRead, SerialPortErrors & outErrors) override;
    HRESULT  GetLines     (SerialPortLines & outLines)                                             override;

private:

    HANDLE  m_handle = INVALID_HANDLE_VALUE;
    bool    m_dtr    = false;
    bool    m_rts    = false;
};
