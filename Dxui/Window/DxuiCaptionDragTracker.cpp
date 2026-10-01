#include "Pch.h"

#include "Window/DxuiCaptionDragTracker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker::OnTick
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionDragTracker::Event DxuiCaptionDragTracker::OnTick (bool buttonDown, SIZE windowSize)
{
    bool  sameSize = false;



    if (!buttonDown || m_resized)
    {
        return Event::None;
    }

    if (!m_dragging)
    {
        m_dragging = true;
        m_reported = false;
        m_size     = windowSize;
    }

    sameSize = windowSize.cx == m_size.cx && windowSize.cy == m_size.cy;

    if (!sameSize)
    {
        //  A resize: whatever the first ticks showed comes down now.
        m_resized = true;

        if (m_reported)
        {
            m_reported = false;
            return Event::Canceled;
        }

        return Event::None;
    }

    m_reported = true;
    return Event::Moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker::OnLoopEnd
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionDragTracker::Event DxuiCaptionDragTracker::OnLoopEnd (SIZE windowSize)
{
    return Finish (windowSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker::OnPoll
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionDragTracker::Event DxuiCaptionDragTracker::OnPoll (bool buttonDown, SIZE windowSize)
{
    if (buttonDown)
    {
        return Event::None;
    }

    return Finish (windowSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker::Rebase
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionDragTracker::Rebase (SIZE windowSize)
{
    if (m_dragging && !m_resized)
    {
        m_size = windowSize;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker::Finish
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionDragTracker::Event DxuiCaptionDragTracker::Finish (SIZE windowSize)
{
    bool  reported = m_reported;
    bool  sameSize = windowSize.cx == m_size.cx && windowSize.cy == m_size.cy;



    if (!m_dragging)
    {
        return Event::None;
    }

    m_dragging = false;
    m_reported = false;
    m_resized  = false;

    if (!reported)
    {
        return Event::None;
    }

    return sameSize ? Event::Ended : Event::Canceled;
}





