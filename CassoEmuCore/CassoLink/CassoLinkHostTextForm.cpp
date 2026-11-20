#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::TakeReceived
//
//  The bytes appended to the buffer, decoded from two characters each once
//  the Apple sends in that form.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::TakeReceived (std::span<const Byte> bytes)
{
    std::vector<Byte> &  buffer = m_adtProServer.m_buffer;
    std::vector<Byte>    pending;



    if (!m_textForm)
    {
        buffer.insert (buffer.end(), bytes.begin(), bytes.end());

        if (buffer.size() < 2 || (buffer[0] & 0x7F) != (kTextHigh | (CassoLinkCodec::kStart >> 4)) || (buffer[1] & 0x7F) != (kTextLow | (CassoLinkCodec::kStart & 0x0F)))
        {
            return;
        }

        m_textForm = true;
        pending.swap (buffer);
        bytes      = pending;
    }

    for (Byte b : bytes)
    {
        b &= 0x7F;

        if ((b & 0xF0) == kTextHigh)
        {
            m_textNibble = b & 0x0F;
        }
        else if ((b & 0xF0) == kTextLow && m_textNibble >= 0)
        {
            buffer.push_back ((Byte) ((m_textNibble << 4) | (b & 0x0F)));
            m_textNibble = -1;
        }
    }
}
