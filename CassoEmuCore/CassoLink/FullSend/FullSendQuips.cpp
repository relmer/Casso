#include "Pch.h"

#include "CassoLink/FullSend/FullSendQuips.h"

#include "CassoLink/FullSend/FullSendScreen.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendQuips::Pick
//
//  Every line once in a random order, then again in a new order that does not
//  begin with the line just picked.
//
////////////////////////////////////////////////////////////////////////////////

Byte FullSendQuips::Pick()
{
    if (m_left.empty())
    {
        for (int line = 1; line <= FullSendScreen::kQuipCount; line++)
        {
            m_left.push_back ((Byte) line);
        }

        std::shuffle (m_left.begin(), m_left.end(), m_random);

        if (m_left.back() == m_last)
        {
            std::swap (m_left.front(), m_left.back());
        }
    }

    m_last = m_left.back();
    m_left.pop_back();

    return m_last;
}
