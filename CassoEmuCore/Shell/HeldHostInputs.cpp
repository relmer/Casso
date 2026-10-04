#include "Pch.h"

#include "Shell/HeldHostInputs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputs::OnPress
//
//  A press that reached the machine holds the input down there. One that did
//  not changes nothing, except that pressing again an input let go of behind
//  live means it is held once more, so it needs no release.
//
////////////////////////////////////////////////////////////////////////////////

void HeldHostInputs::OnPress (WPARAM input, bool isLive)
{
    if (isLive)
    {
        m_held.insert (input);
        m_released.erase (input);
    }
    else if (m_released.erase (input) != 0)
    {
        m_held.insert (input);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputs::OnRelease
//
//  A release that reached the machine needs no other; one kept from it by
//  the gate is owed if the machine has the input down.
//
////////////////////////////////////////////////////////////////////////////////

void HeldHostInputs::OnRelease (WPARAM input, bool isLive)
{
    bool  wasHeld = m_held.erase (input) != 0;



    if (isLive)
    {
        m_released.erase (input);
    }
    else if (wasHeld)
    {
        m_released.insert (input);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputs::OnReleaseAll
//
//  Every input let go of at once, as when the window loses focus.
//
////////////////////////////////////////////////////////////////////////////////

void HeldHostInputs::OnReleaseAll (bool isLive)
{
    if (isLive)
    {
        m_released.clear();
    }
    else
    {
        m_released.insert (m_held.begin(), m_held.end());
    }

    m_held.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputs::TakeReleases
//
//  The releases owed, in key-code order, each given once.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<WPARAM> HeldHostInputs::TakeReleases()
{
    std::vector<WPARAM>  releases (m_released.begin(), m_released.end());



    m_released.clear();

    return releases;
}
