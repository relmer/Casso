#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelLineBuffer
//
//  Bytes read from the debug channel, split into LF-terminated lines. A
//  partial line is limited to ChannelProtocol::kMaxLineBytes, so a peer that
//  sends bytes with no LF cannot grow the buffer without bound.
//
////////////////////////////////////////////////////////////////////////////////

class ChannelLineBuffer
{
public:
    //  Adds bytes read from the pipe. False, with the buffer emptied, when the
    //  unfinished line after them is longer than kMaxLineBytes.
    bool  TryAppend   (const char * bytes, size_t count);

    //  The next complete line, without its LF or a CR before the LF.
    bool  TryTakeLine (std::string & line);

private:
    std::string  m_pending;
    size_t       m_partialBytes = 0;
};
