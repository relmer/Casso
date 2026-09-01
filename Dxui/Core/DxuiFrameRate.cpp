#include "Pch.h"

#include "Core/DxuiFrameRate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFrameRate::Tick
//
//  The average is over the window's OWN elapsed time, not over kWindowSeconds,
//  because the two differ: the frame that closes the window overshoots it, and
//  dividing by the nominal second instead of the real elapsed one reports a
//  rate slightly higher than the machine achieved. At sixty frames that is a
//  fraction of a frame and at four it is not.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFrameRate::Tick (float deltaSeconds)
{
    if (deltaSeconds <= 0.0f)
    {
        return;
    }

    m_elapsed += deltaSeconds;
    m_frames++;

    if (m_elapsed >= kWindowSeconds)
    {
        m_framesPerSecond = (float) m_frames / m_elapsed;
        m_elapsed         = 0.0f;
        m_frames          = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFrameRate::Reset
//
//  The last completed average SURVIVES a reset. It is what the readout is
//  showing, and blanking it to zero because a modal loop just ended would put
//  a wrong number on screen for a second rather than a stale one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFrameRate::Reset()
{
    m_elapsed = 0.0f;
    m_frames  = 0;
}
