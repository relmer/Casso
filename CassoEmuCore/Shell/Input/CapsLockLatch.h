#pragma once

#include "Pch.h"

#include "Seams/IHostCapsLock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapsLockLatch
//
//  The Caps Lock key an Apple ][ ships latched down, kept as a bool the
//  emulator owns and driven onto the host keyboard while the emulator window
//  has focus. The host LED is then the single source of truth: it reads as
//  the emulated key, and the user moves it with the real key.
//
//  Gaining focus parks the host's own toggle and sets the host to the latch.
//  Losing focus sets the host back to what was parked. Parking happens on
//  every gain, not only the first, so a toggle the user made in another
//  program while the emulator was in the background is what comes back.
//
//  A VK_CAPITAL key-up the user pressed becomes the new latch value. The
//  key-up produced by our own flip is recognized through the seam and leaves
//  the latch alone.
//
////////////////////////////////////////////////////////////////////////////////

class CapsLockLatch
{
public:

    explicit CapsLockLatch (IHostCapsLock & host) : m_host (host) {}

    void  OnGainedFocus   ();
    void  OnLostFocus     ();
    void  OnCapsLockKeyUp ();

    bool  IsLatched () const { return m_isLatched; }
    bool  HasFocus  () const { return m_hasFocus; }

private:

    void  DriveHostTo (bool on);

    IHostCapsLock &  m_host;
    bool             m_isLatched = true;    // a real //e starts with the key down
    bool             m_hasFocus  = false;
    bool             m_parked    = false;   // the host's own toggle while we hold focus
};
