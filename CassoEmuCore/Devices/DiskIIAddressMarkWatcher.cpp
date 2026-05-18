#include "Pch.h"

#include "DiskIIAddressMarkWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveNibble
//
//  Feeds one nibble to both the address-mark and the data-mark state
//  machines. Each call is wait-free; the watcher MUST NOT mutate the
//  nibble stream or back-pressure the controller (FR-008). Fires sink
//  notifications synchronously from the per-state helpers when a
//  terminal-accept condition is met.
//
//  PRECONDITION: callers MUST gate on the LSS "byte ready" rising
//  edge (DiskIINibbleEngine::ConsumeFreshNibble) and pass each
//  assembled nibble exactly once. Feeding every CPU poll of $C0EC
//  here floods the state machines with repeated bytes and partial-
//  assembly garbage, and zero real D5 AA 96 / D5 AA AD prologues
//  ever match. Spec-006 regression: see DiskIIController::Read.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAddressMarkWatcher::ObserveNibble (uint8_t nibble) noexcept
{
    StepAddrMarkState (nibble);
    StepDataMarkState (nibble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepAddrMarkState
//
//  Walks the 4-and-4 address-mark state machine one nibble at a time.
//  Prologue is D5 AA 96; eight field nibbles follow (vol/trk/sec/chk in
//  hi/lo pairs). On the ChkLo terminal the watcher verifies the
//  Beneath-Apple-DOS XOR checksum (chk == vol XOR trk XOR sec) and
//  fires OnAddressMark only on a checksum hit. Random nibble streams
//  occasionally produce a spurious D5 AA 96 triplet; the checksum is
//  the false-positive guard (research.md §1.3).
//
//  Any nibble that breaks the expected sequence resets to Idle, with
//  one overlap shortcut: an unexpected D5 restarts at SawD5 instead of
//  Idle so back-to-back marks (D5 D5 AA 96 ...) re-anchor correctly.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAddressMarkWatcher::StepAddrMarkState (uint8_t nibble) noexcept
{
    uint8_t  vol = 0;
    uint8_t  trk = 0;
    uint8_t  sec = 0;
    uint8_t  chk = 0;
    uint8_t  chkLo = 0;



    switch (m_addrState)
    {
        case AddrState::Idle:
            if (nibble == kAddrMarkPrologue0)
            {
                m_addrState = AddrState::SawD5;
            }
            break;

        case AddrState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                m_addrState = AddrState::SawAA;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                // Stay; D5 D5 still gives us a fresh prologue lead-in.
            }
            else
            {
                m_addrState = AddrState::Idle;
            }
            break;

        case AddrState::SawAA:
            if (nibble == kAddrMarkPrologue2)
            {
                m_addrState = AddrState::VolHi;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                m_addrState = AddrState::SawD5;
            }
            else
            {
                m_addrState = AddrState::Idle;
            }
            break;

        case AddrState::VolHi:
            m_volHi     = nibble;
            m_addrState = AddrState::VolLo;
            break;

        case AddrState::VolLo:
            m_volLo     = nibble;
            m_addrState = AddrState::TrkHi;
            break;

        case AddrState::TrkHi:
            m_trkHi     = nibble;
            m_addrState = AddrState::TrkLo;
            break;

        case AddrState::TrkLo:
            m_trkLo     = nibble;
            m_addrState = AddrState::SecHi;
            break;

        case AddrState::SecHi:
            m_secHi     = nibble;
            m_addrState = AddrState::SecLo;
            break;

        case AddrState::SecLo:
            m_secLo     = nibble;
            m_addrState = AddrState::ChkHi;
            break;

        case AddrState::ChkHi:
            m_chkHi     = nibble;
            m_addrState = AddrState::ChkLo;
            break;

        case AddrState::ChkLo:
            chkLo       = nibble;
            vol         = Decode4and4 (m_volHi, m_volLo);
            trk         = Decode4and4 (m_trkHi, m_trkLo);
            sec         = Decode4and4 (m_secHi, m_secLo);
            chk         = Decode4and4 (m_chkHi, chkLo);

            if (chk == (uint8_t) (vol ^ trk ^ sec))
            {
                m_cachedSector = (int) sec;

                if (m_eventSink != nullptr)
                {
                    m_eventSink->OnAddressMark ((int) trk, (int) sec, (int) vol);
                }
            }

            m_addrState = AddrState::Idle;
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepDataMarkState
//
//  Walks the 6-and-2 data-mark state machine one nibble at a time.
//  Prologue is D5 AA AD; body is 342 6-and-2 nibbles + 1 checksum
//  nibble; epilogue is DE AA EB. The watcher counts body nibbles only
//  to bound the search window — it does NOT verify the body checksum
//  (research.md §1.3: that would require 342 nibbles of buffering and
//  the false-positive cost of skipping it is just a spurious "DATA
//  READ" entry, which is harmless).
//
//  Epilogue detection uses a 3-nibble rolling window (m_peek2,
//  m_peek1, m_peek0). Once we're in Body, every incoming nibble shifts
//  the window forward; a window match of DE AA EB fires
//  OnDataMarkRead. The cached sector number from the most recent
//  OnAddressMark accompanies the fire; if no address mark preceded
//  this data mark the cached value is -1 and the UI formats it as
//  "S?" (research.md / spec FR-008).
//
//  Slack guard: if the body runs past kDataNibbleCount +
//  kDataNibbleCountSlack without an epilogue hit we silently reset to
//  Idle. A protected loader may produce no epilogue at all; this guard
//  keeps the state machine from latching forever.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAddressMarkWatcher::StepDataMarkState (uint8_t nibble) noexcept
{
    m_peek2 = m_peek1;
    m_peek1 = m_peek0;
    m_peek0 = nibble;



    switch (m_dataState)
    {
        case DataState::Idle:
            if (nibble == kAddrMarkPrologue0)
            {
                m_dataState = DataState::SawD5;
            }
            break;

        case DataState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                m_dataState = DataState::SawAA;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                // Stay; D5 D5 still gives us a fresh prologue lead-in.
            }
            else
            {
                m_dataState = DataState::Idle;
            }
            break;

        case DataState::SawAA:
            if (nibble == kDataMarkPrologue2)
            {
                m_dataState    = DataState::Body;
                m_dataNibCount = 0;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                m_dataState = DataState::SawD5;
            }
            else
            {
                m_dataState = DataState::Idle;
            }
            break;

        case DataState::Body:
            m_dataNibCount++;

            if (m_peek2 == kSectorEpilogue0 &&
                m_peek1 == kSectorEpilogue1 &&
                m_peek0 == kSectorEpilogue2)
            {
                if (m_eventSink != nullptr)
                {
                    m_eventSink->OnDataMarkRead (m_cachedSector, 256);
                }

                m_dataState = DataState::Idle;
            }
            else if (m_dataNibCount > (kDataNibbleCount + kDataNibbleCountSlack))
            {
                m_dataState = DataState::Idle;
            }
            break;
    }
}
