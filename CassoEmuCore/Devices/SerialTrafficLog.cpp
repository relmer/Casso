#include "Pch.h"

#include "Devices/SerialTrafficLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialTrafficLog::SerialTrafficLog
//
////////////////////////////////////////////////////////////////////////////////

SerialTrafficLog::SerialTrafficLog (WriteFn write) :
    m_write (std::move (write))
{
    m_write ("# Casso serial log: cycle, port, > from the Apple or < to it, bytes in hex");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialTrafficLog::~SerialTrafficLog
//
////////////////////////////////////////////////////////////////////////////////

SerialTrafficLog::~SerialTrafficLog()
{
    m_write ("\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialTrafficLog::Append
//
////////////////////////////////////////////////////////////////////////////////

void SerialTrafficLog::Append (int port, bool fromGuest, Byte value, uint64_t cycle)
{
    bool  newLine = m_lineBytes == 0
                 || m_lineBytes == kBytesPerLine
                 || port      != m_port
                 || fromGuest != m_fromGuest
                 || cycle - m_lastCycle > kPauseCycles;



    if (newLine)
    {
        m_write (std::format ("\n{:>12} {} {}", cycle, port, fromGuest ? '>' : '<'));

        m_port      = port;
        m_fromGuest = fromGuest;
        m_lineBytes = 0;
    }

    m_write (std::format (" {:02X}", value));

    m_lineBytes++;
    m_lastCycle = cycle;
}
