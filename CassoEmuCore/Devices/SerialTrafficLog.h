#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialTrafficLog
//
//  Text lines of the bytes crossing the emulated serial ports, for protocol
//  captures (`--serial-log <file>`). After a comment line, each line is
//
//      <cycle> <port> <direction> <bytes>
//
//  The cycle is the line's own count when the first byte crossed; `>` marks
//  bytes from the Apple and `<` bytes to it; the bytes are hex, up to 32 to
//  a line. A new line starts when the port or the direction changes, after
//  32 bytes, or after a pause of more than kPauseCycles.
//
//  Each byte is written as it crosses, so every byte sent before a crash is
//  in the file.
//
////////////////////////////////////////////////////////////////////////////////

class SerialTrafficLog
{
public:
    using WriteFn = std::function<void (std::string_view text)>;

    static constexpr size_t    kBytesPerLine = 32;
    static constexpr uint64_t  kPauseCycles  = 10000;

    explicit SerialTrafficLog (WriteFn write);
    ~SerialTrafficLog ();

    void  Append (int port, bool fromGuest, Byte value, uint64_t cycle);

private:
    WriteFn   m_write;
    int       m_port      = 0;
    bool      m_fromGuest = false;
    size_t    m_lineBytes = 0;
    uint64_t  m_lastCycle = 0;
};
