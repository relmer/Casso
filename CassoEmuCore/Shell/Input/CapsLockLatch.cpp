#include "Pch.h"

#include "Shell/Input/CapsLockLatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OnGainedFocus
//
//  A second gain without a loss between is ignored, so the parked value is
//  always the host's own and never a value we set ourselves.
//
////////////////////////////////////////////////////////////////////////////////

void CapsLockLatch::OnGainedFocus()
{
    if (m_hasFocus)
    {
        return;
    }

    m_hasFocus = true;
    m_parked   = m_host.IsOn();

    DriveHostTo (m_isLatched);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLostFocus
//
//  Idempotent for the same reason: exit calls it after focus may already
//  have gone, and restoring twice would flip the host back to the latch.
//
////////////////////////////////////////////////////////////////////////////////

void CapsLockLatch::OnLostFocus()
{
    if (!m_hasFocus)
    {
        return;
    }

    m_hasFocus = false;

    DriveHostTo (m_parked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCapsLockKeyUp
//
//  The toggle has already flipped by the time the key-up arrives, so the
//  host's current value is the user's choice. Our own flips arrive by the
//  same route and are skipped; a key-up seen without focus belongs to a
//  toggle made for some other window and is skipped too.
//
////////////////////////////////////////////////////////////////////////////////

void CapsLockLatch::OnCapsLockKeyUp()
{
    bool  isOurs = m_host.WasLastKeySynthesized();



    if (isOurs || !m_hasFocus)
    {
        return;
    }

    m_isLatched = m_host.IsOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveHostTo
//
////////////////////////////////////////////////////////////////////////////////

void CapsLockLatch::DriveHostTo (bool on)
{
    bool  isOn = m_host.IsOn();



    if (isOn != on)
    {
        m_host.Toggle();
    }
}
