#include "Pch.h"

#include "Debugger/Channel/ChannelLineBuffer.h"

#include "Debugger/Channel/ChannelProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelLineBuffer::TryAppend
//
//  The length of the unfinished line is counted as bytes arrive, so a long
//  line costs one pass over its bytes rather than a search per read.
//
////////////////////////////////////////////////////////////////////////////////

bool ChannelLineBuffer::TryAppend (const char * bytes, size_t count)
{
    bool  isTooLong = false;



    for (size_t i = 0; i < count; i++)
    {
        m_partialBytes = (bytes[i] == '\n') ? 0 : m_partialBytes + 1;
    }

    isTooLong = m_partialBytes > ChannelProtocol::kMaxLineBytes;

    if (isTooLong)
    {
        m_pending.clear();
        m_partialBytes = 0;
    }
    else
    {
        m_pending.append (bytes, count);
    }

    return !isTooLong;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelLineBuffer::TryTakeLine
//
////////////////////////////////////////////////////////////////////////////////

bool ChannelLineBuffer::TryTakeLine (std::string & line)
{
    size_t  newline = m_pending.find ('\n');



    if (newline == std::string::npos)
    {
        return false;
    }

    line = m_pending.substr (0, newline);
    m_pending.erase (0, newline + 1);

    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    return true;
}
