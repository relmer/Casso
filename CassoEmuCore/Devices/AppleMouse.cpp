#include "Pch.h"

#include "AppleMouse.h"
#include "Core/MemoryBus.h"
#include "Video/IVideoTiming.h"




////////////////////////////////////////////////////////////////////////////////
//
//  AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleMouse::AttachInterruptController (IInterruptController * ic)
{
    HRESULT   hr = S_OK;

    CBR (ic != nullptr);

    hr = ic->RegisterSource (m_xySource);
    CHR (hr);

    hr = ic->RegisterSource (m_vblSource);
    CHR (hr);

    m_ic       = ic;
    m_irqBound = true;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MoveBy
//
//  Host thread. Accumulates signed movement deltas; the CPU-thread Tick
//  drains them into the latch pipeline one unit per axis at a time.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::MoveBy (int dx, int dy)
{
    if (dx != 0)
    {
        m_hostDx.fetch_add (dx, std::memory_order_acq_rel);
    }
    if (dy != 0)
    {
        m_hostDy.fetch_add (dy, std::memory_order_acq_rel);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  CPU thread, once per instruction via the EmuCpu cycle fan-out. Two jobs:
//
//  1. VBL latch: on the display->vblank transition, latch the VBL interrupt
//     flag ($C019 bit 7). Latched regardless of ENVBL (the enable masks only
//     the IRQ line); cleared by a $C070 read.
//
//  2. Movement latches: drain the host deltas, then, for each axis with no
//     unacknowledged interrupt, latch one movement unit -- direction line
//     ($C066/$C067 bit 7) plus pending flag ($C015/$C017 bit 7). The
//     firmware's service loop steps position exactly +/-1 per latched axis
//     and acknowledges via $C048, which frees the latch for the next unit.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::Tick (uint32_t cpuCycles)
{
    // Absolute targeting: periodically re-derive the pending motion from
    // the host target fraction + the firmware's live screen holes.
    if (m_retargetCountdown <= cpuCycles)
    {
        m_retargetCountdown = kRetargetIntervalCycles;
        RetargetFromHoles();
    }
    else
    {
        m_retargetCountdown -= cpuCycles;
    }

    // VBL onset edge.
    if (m_videoTiming != nullptr)
    {
        bool  inVblank = m_videoTiming->IsInVblank();

        if (inVblank && !m_lastInVblank)
        {
            m_vblInt = true;
        }
        m_lastInVblank = inVblank;
    }

    // Drain host motion into the CPU-side queue. Clamp the backlog to the
    // firmware's maximum clamp span: real hardware doesn't queue at all
    // (unserviced pulses are simply lost), so an unbounded backlog while
    // the guest has interrupts masked would replay a stale burst later.
    {
        constexpr int  kMaxPending = 1023;

        int  dx = m_hostDx.exchange (0, std::memory_order_acq_rel);
        int  dy = m_hostDy.exchange (0, std::memory_order_acq_rel);

        m_pendingX = std::clamp (m_pendingX + dx, -kMaxPending, kMaxPending);
        m_pendingY = std::clamp (m_pendingY + dy, -kMaxPending, kMaxPending);
    }

    // Latch one unit per idle axis. MOUX1 bit 7 = 1 -> firmware increments X
    // (+X = right). MOUY1 is inverted by the firmware (EOR #$80 before the
    // shared direction test), so bit 7 = 0 -> firmware increments Y (+Y = down).
    if (!m_xInt && m_pendingX != 0)
    {
        m_mouX1     = (m_pendingX > 0) ? 0x80 : 0x00;
        m_pendingX += (m_pendingX > 0) ? -1 : +1;
        m_xInt      = true;
    }

    if (!m_yInt && m_pendingY != 0)
    {
        m_mouY1     = (m_pendingY > 0) ? 0x00 : 0x80;
        m_pendingY += (m_pendingY > 0) ? -1 : +1;
        m_yInt      = true;
    }

    UpdateIrqLines();
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetHostTargetFraction
//
//  Host thread. Latest-wins packed store; Tick consumes it on the CPU
//  thread at the retarget cadence.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::SetHostTargetFraction (uint16_t fx, uint16_t fy)
{
    m_hostTarget.store ((static_cast<uint32_t> (fx) << 16) | fy,
                        std::memory_order_release);
    m_hasTarget.store (true, std::memory_order_release);
}




////////////////////////////////////////////////////////////////////////////////
//
//  RetargetFromHoles
//
//  CPU thread. Projects the host viewport fraction into the mouse
//  firmware's LIVE clamp window and REPLACES the pending motion with the
//  delta from the firmware's current position (both read from the slot-7
//  screen holes over the bus — same thread as guest execution, so the
//  reads are race-free and see the live MMU mapping). A latched-but-
//  unacknowledged unit is counted as already applied. Self-correcting:
//  anything the firmware clamps away re-derives on the next pass. The
//  hole sanity checks make this inert until the guest app has initialized
//  the mouse firmware (pre-INITMOUSE holes are garbage).
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::RetargetFromHoles()
{
    if (m_bus == nullptr || !m_hasTarget.load (std::memory_order_acquire))
    {
        return;
    }

    uint32_t  packed = m_hostTarget.load (std::memory_order_acquire);
    int       fx     = static_cast<int> (packed >> 16);
    int       fy     = static_cast<int> (packed & 0xFFFF);

    auto rd16 = [this] (Word lo, Word hi)
    {
        return static_cast<int> (m_bus->ReadByte (lo))
             | (static_cast<int> (m_bus->ReadByte (hi)) << 8);
    };

    int  xMin = rd16 (kHoleXMinLo, kHoleXMinHi);
    int  xMax = rd16 (kHoleXMaxLo, kHoleXMaxHi);
    int  yMin = rd16 (kHoleYMinLo, kHoleYMinHi);
    int  yMax = rd16 (kHoleYMaxLo, kHoleYMaxHi);
    int  curX = rd16 (kHoleXPosLo, kHoleXPosHi);
    int  curY = rd16 (kHoleYPosLo, kHoleYPosHi);

    // Sanity: a live clamp window is ordered, spans at most the firmware's
    // 0..1023 default range, and contains the current position.
    if (xMax <= xMin || yMax <= yMin ||
        xMax - xMin > 1023 || yMax - yMin > 1023 ||
        curX < xMin || curX > xMax || curY < yMin || curY > yMax)
    {
        return;
    }

    int  targetX = xMin + (fx * (xMax - xMin)) / 65535;
    int  targetY = yMin + (fy * (yMax - yMin)) / 65535;

    // A latched, unacknowledged unit will land as +/-1 when the firmware
    // services it; count it as already applied so it isn't double-queued.
    int  inFlightX = m_xInt ? (((m_mouX1 & 0x80) != 0) ? +1 : -1) : 0;
    int  inFlightY = m_yInt ? (((m_mouY1 & 0x80) != 0) ? -1 : +1) : 0;

    m_pendingX = (targetX - curX) - inFlightX;
    m_pendingY = (targetY - curY) - inFlightY;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadButton
//
//  $C063 bit 7, ACTIVE LOW: the button line idles high (0x80) and a press
//  pulls it to 0. (The firmware sets its button-down status bit when a
//  BIT $C063 leaves N clear.) On the //c this line replaces the //e's
//  shift-key mod -- the keyboard forwards $C063 here when a mouse exists.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleMouse::ReadButton() const
{
    return m_hostButton.load (std::memory_order_acquire) ? 0x00 : 0x80;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AccessRstXY
//
//  $C048 (any access): clear both movement-interrupt latches. The next
//  pending unit (if any) loads on a later Tick, re-asserting the line --
//  one interrupt per movement unit, never a double-fire for the same unit.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::AccessRstXY()
{
    m_xInt = false;
    m_yInt = false;

    UpdateIrqLines();
}




////////////////////////////////////////////////////////////////////////////////
//
//  AccessPtrig
//
//  $C070 read side effect (//c only): clears the VBL interrupt latch.
//  The paddle-timer trigger itself stays with the soft-switch bank.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::AccessPtrig()
{
    m_vblInt = false;

    UpdateIrqLines();
}




////////////////////////////////////////////////////////////////////////////////
//
//  AccessIouSwitch
//
//  $C058-$C05F while IOU access is enabled ($C079). Any access programs the
//  switch (the firmware uses STA). Edge selectors are stored for fidelity
//  but do not alter behavior: the emulation synthesizes one interrupt per
//  movement unit at whichever polarity is selected.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::AccessIouSwitch (Word address)
{
    switch (address & 0x0007)
    {
    case 0x0: m_xyEnabled     = false; break;   // $C058 DISXY
    case 0x1: m_xyEnabled     = true;  break;   // $C059 ENBXY
    case 0x2: m_vblEnabled    = false; break;   // $C05A DISVBL
    case 0x3: m_vblEnabled    = true;  break;   // $C05B ENVBL
    case 0x4: m_x0EdgeFalling = false; break;   // $C05C X0EDGE rising
    case 0x5: m_x0EdgeFalling = true;  break;   // $C05D X0EDGE falling
    case 0x6: m_y0EdgeFalling = false; break;   // $C05E Y0EDGE rising
    case 0x7: m_y0EdgeFalling = true;  break;   // $C05F Y0EDGE falling
    }

    UpdateIrqLines();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Power-on / reset state: latches clear, interrupts masked, IOU access
//  off (so $C058-$C05F revert to the annunciator/DHIRES bank behavior),
//  pending motion discarded.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::Reset()
{
    m_hostDx.store (0, std::memory_order_release);
    m_hostDy.store (0, std::memory_order_release);
    m_hasTarget.store (false, std::memory_order_release);
    m_retargetCountdown = 0;

    m_pendingX         = 0;
    m_pendingY         = 0;
    m_xInt             = false;
    m_yInt             = false;
    m_vblInt           = false;
    m_mouX1            = 0;
    m_mouY1            = 0;
    m_xyEnabled        = false;
    m_vblEnabled       = false;
    m_x0EdgeFalling    = false;
    m_y0EdgeFalling    = false;
    m_iouAccessEnabled = false;
    m_lastInVblank     = false;

    UpdateIrqLines();
}




////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIrqLines
//
//  Level-sensitive aggregation: each line is held while `enabled && latched`
//  and drops on acknowledge ($C048 / $C070) or mask (DISXY / DISVBL).
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::UpdateIrqLines()
{
    if (!m_irqBound || m_ic == nullptr)
    {
        return;
    }

    if (m_xyEnabled && (m_xInt || m_yInt))
    {
        m_ic->Assert (m_xySource);
    }
    else
    {
        m_ic->Clear (m_xySource);
    }

    if (m_vblEnabled && m_vblInt)
    {
        m_ic->Assert (m_vblSource);
    }
    else
    {
        m_ic->Clear (m_vblSource);
    }
}
