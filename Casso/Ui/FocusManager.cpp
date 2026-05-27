#include "Pch.h"

#include "FocusManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void FocusManager::Clear ()
{
    m_order.clear();
    m_currentIndex = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterFocusable
//
////////////////////////////////////////////////////////////////////////////////

void FocusManager::RegisterFocusable (int focusId)
{
    m_order.push_back (focusId);

    if (m_currentIndex < 0)
    {
        m_currentIndex = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Current
//
////////////////////////////////////////////////////////////////////////////////

int FocusManager::Current () const
{
    if ((m_currentIndex < 0) || (m_currentIndex >= (int) m_order.size()))
    {
        return -1;
    }

    return m_order[m_currentIndex];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocus
//
////////////////////////////////////////////////////////////////////////////////

bool FocusManager::SetFocus (int focusId)
{
    for (size_t i = 0; i < m_order.size(); i++)
    {
        if (m_order[i] == focusId)
        {
            m_currentIndex = (int) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceForward
//
////////////////////////////////////////////////////////////////////////////////

bool FocusManager::AdvanceForward ()
{
    if (m_order.empty())
    {
        return false;
    }

    m_currentIndex = (m_currentIndex + 1) % (int) m_order.size();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceBackward
//
////////////////////////////////////////////////////////////////////////////////

bool FocusManager::AdvanceBackward ()
{
    int  count = (int) m_order.size();



    if (count == 0)
    {
        return false;
    }

    m_currentIndex = ((m_currentIndex <= 0) ? count : m_currentIndex) - 1;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool FocusManager::HandleKey (FocusKey key)
{
    int  focusedId = 0;



    switch (key)
    {
    case FocusKey::Tab:
        return AdvanceForward();

    case FocusKey::ShiftTab:
        return AdvanceBackward();

    case FocusKey::Enter:
    case FocusKey::Space:
        focusedId = Current();

        if ((focusedId >= 0) && m_activate)
        {
            m_activate (focusedId);
            return true;
        }
        return false;

    case FocusKey::Escape:
        if (m_dismiss)
        {
            m_dismiss();
            return true;
        }
        return false;

    case FocusKey::None:
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyKey
//
////////////////////////////////////////////////////////////////////////////////

FocusKey FocusManager::ClassifyKey (UINT vkCode, bool shiftHeld)
{
    switch (vkCode)
    {
    case VK_TAB:    return shiftHeld ? FocusKey::ShiftTab : FocusKey::Tab;
    case VK_RETURN: return FocusKey::Enter;
    case VK_SPACE:  return FocusKey::Space;
    case VK_ESCAPE: return FocusKey::Escape;
    }

    return FocusKey::None;
}
