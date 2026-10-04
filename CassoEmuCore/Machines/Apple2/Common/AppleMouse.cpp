#include "Pch.h"

#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/MemoryBus.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Machines/Apple2/Common/IVideoTiming.h"





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
    bool  irqChanged = false;



    // Batch the bookkeeping that does not need per-instruction resolution --
    // the retarget countdown, the VBL onset sample, and the host-motion drain
    // -- onto kSampleQuantum. The movement LATCH stays per-instruction below,
    // so a queued unit reaches the firmware the moment the previous one is
    // acknowledged and the cursor tracks without lag.
    m_sampleAccum += cpuCycles;
    if (m_sampleAccum >= kSampleQuantum)
    {
        uint32_t  cyc = m_sampleAccum;
        m_sampleAccum = 0;

        if (m_retargetCountdown <= cyc)
        {
            m_retargetCountdown = kRetargetIntervalCycles;
            RetargetFromHoles (cpuCycles);
        }
        else
        {
            m_retargetCountdown -= cyc;
        }

        if (m_videoTiming != nullptr)
        {
            bool  inVblank = m_videoTiming->IsInVblank();

            if (inVblank && !m_lastInVblank)
            {
                m_vblInt   = true;
                irqChanged = true;
            }

            m_lastInVblank = inVblank;
        }

        if (m_hostDx.load (std::memory_order_relaxed) != 0 ||
            m_hostDy.load (std::memory_order_relaxed) != 0)
        {
            constexpr int  kMaxPending = 1023;

            int  dx = m_hostDx.exchange (0, std::memory_order_acq_rel);
            int  dy = m_hostDy.exchange (0, std::memory_order_acq_rel);

            // Both deltas in one record, X in the high half. A slice-boundary
            // sample may already have recorded exactly this motion.
            if (m_inputJournal != nullptr && (dx != m_observedDx || dy != m_observedDy))
            {
                m_inputJournal->RecordObserved (m_inputJournal->GetCycle() - cpuCycles, InputKind::MouseMove, 0, 0, PackMotion (dx, dy));
            }

            m_observedDx = 0;
            m_observedDy = 0;

            m_pendingX = std::clamp (m_pendingX + dx, -kMaxPending, kMaxPending);
            m_pendingY = std::clamp (m_pendingY + dy, -kMaxPending, kMaxPending);
        }
    }

    // Latch one unit per idle axis. MOUX1 bit 7 = 1 -> firmware increments X
    // (+X = right). MOUY1 is inverted by the firmware (EOR #$80 before the
    // shared direction test), so bit 7 = 0 -> firmware increments Y (+Y = down).
    if (!m_xInt && m_pendingX != 0)
    {
        m_mouX1     = (m_pendingX > 0) ? 0x80 : 0x00;
        m_pendingX += (m_pendingX > 0) ? -1 : +1;
        m_xInt      = true;
        irqChanged  = true;
    }

    if (!m_yInt && m_pendingY != 0)
    {
        m_mouY1     = (m_pendingY > 0) ? 0x00 : 0x80;
        m_pendingY += (m_pendingY > 0) ? -1 : +1;
        m_yInt      = true;
        irqChanged  = true;
    }

    // The only IRQ-line inputs that change inside Tick are the VBL and X/Y
    // latches set above; acknowledges ($C048/$C070) and mask changes
    // ($C058-$C05F) drive UpdateIrqLines from their own handlers. So refresh
    // the lines only when this tick actually latched something -- skipping the
    // call on the common no-change tick avoids two interrupt-controller calls
    // per instruction.
    if (irqChanged)
    {
        UpdateIrqLines();
    }
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

void AppleMouse::RetargetFromHoles (uint32_t cpuCycles)
{
    bool      hasTarget = m_hasTarget.load (std::memory_order_acquire);
    uint32_t  target    = m_hostTarget.load (std::memory_order_acquire);



    // The flag and the position are one input: a change to either records
    // both, as this pass read them.
    if (m_inputJournal != nullptr && (hasTarget != m_observedHasTarget || target != m_observedTarget))
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle() - cpuCycles, InputKind::MouseTarget, hasTarget ? 1 : 0, 0, target);
        m_observedHasTarget = hasTarget;
        m_observedTarget    = target;
    }

    // No bus, or no host position staged: nothing to project.
    if (m_bus != nullptr && hasTarget)
    {
        uint32_t  packed = target;
        int       fx     = static_cast<int> (packed >> 16);
        int       fy     = static_cast<int> (packed & 0xFFFF);
        int       xMin   = 0;
        int       xMax   = 0;
        int       yMin   = 0;
        int       yMax   = 0;
        int       curX   = 0;
        int       curY   = 0;

        auto rd16 = [this] (Word lo, Word hi)
        {
            return static_cast<int> (m_bus->ReadByte (lo))
                 | (static_cast<int> (m_bus->ReadByte (hi)) << 8);
        };

        xMin = rd16 (kHoleXMinLo, kHoleXMinHi);
        xMax = rd16 (kHoleXMaxLo, kHoleXMaxHi);
        yMin = rd16 (kHoleYMinLo, kHoleYMinHi);
        yMax = rd16 (kHoleYMaxLo, kHoleYMaxHi);
        curX = rd16 (kHoleXPosLo, kHoleXPosHi);
        curY = rd16 (kHoleYPosLo, kHoleYPosHi);

        // Sanity: a live clamp window is ordered, spans at most the firmware's
        // 0..1023 default range, and contains the current position. Failing
        // any of these means the holes are pre-INITMOUSE garbage, so the
        // retarget is skipped entirely rather than acting on nonsense.
        bool  holesLive = xMax > xMin && yMax > yMin
                          && xMax - xMin <= 1023 && yMax - yMin <= 1023
                          && curX >= xMin && curX <= xMax
                          && curY >= yMin && curY <= yMax;

        if (holesLive)
        {
            int  targetX = xMin + (fx * (xMax - xMin)) / 65535;
            int  targetY = yMin + (fy * (yMax - yMin)) / 65535;

            // A latched, unacknowledged unit will land as +/-1 when the
            // firmware services it; count it as already applied so it isn't
            // double-queued.
            int  inFlightX = m_xInt ? (((m_mouX1 & 0x80) != 0) ? +1 : -1) : 0;
            int  inFlightY = m_yInt ? (((m_mouY1 & 0x80) != 0) ? -1 : +1) : 0;

            m_pendingX = (targetX - curX) - inFlightX;
            m_pendingY = (targetY - curY) - inFlightY;
        }
    }
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
    bool  isDown = m_hostButton.load (std::memory_order_acquire);



    if (m_inputJournal != nullptr)
    {
        ObserveButton (isDown);
    }

    return isDown ? 0x00 : 0x80;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveButton
//
//  Journal attached. Records the button when another thread changed it since
//  the last read that saw it; out of line, so a read with the journal off
//  pays only the null test.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void AppleMouse::ObserveButton (bool isDown) const
{
    if (isDown != m_observedButton)
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::MouseButton, isDown ? 1 : 0, 0, 0);
        m_observedButton = isDown;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputJournal
//
//  The host state at the moment of attaching is what a keyframe taken then
//  holds, so it counts as seen.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::SetInputJournal (InputJournal * journal)
{
    m_inputJournal = journal;
    SyncObservedInputs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInput
//
//  Motion is stored, not added: the record holds everything the drain took,
//  and with no host thread writing during a replay there is nothing else.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleMouse::ApplyInput (const InputRecord & record)
{
    bool  isApplied = true;



    switch (record.kind)
    {
        case InputKind::MouseMove:
            m_hostDx.store (static_cast<int> (static_cast<uint32_t> (record.data >> 32)), std::memory_order_release);
            m_hostDy.store (static_cast<int> (static_cast<uint32_t> (record.data)),       std::memory_order_release);
            m_observedDx = static_cast<int> (static_cast<uint32_t> (record.data >> 32));
            m_observedDy = static_cast<int> (static_cast<uint32_t> (record.data));
            break;

        case InputKind::MouseButton:
            m_hostButton.store (record.value != 0, std::memory_order_release);
            m_observedButton = record.value != 0;
            break;

        case InputKind::MouseTarget:
            m_hostTarget.store (static_cast<uint32_t> (record.data), std::memory_order_release);
            m_hasTarget.store  (record.value != 0,                    std::memory_order_release);
            m_observedTarget    = static_cast<uint32_t> (record.data);
            m_observedHasTarget = record.value != 0;
            break;

        default:
            isApplied = false;
            break;
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncObservedInputs
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::SyncObservedInputs()
{
    m_observedButton    = m_hostButton.load (std::memory_order_acquire);
    m_observedHasTarget = m_hasTarget.load  (std::memory_order_acquire);
    m_observedTarget    = m_hostTarget.load (std::memory_order_acquire);
    m_observedDx        = m_hostDx.load     (std::memory_order_acquire);
    m_observedDy        = m_hostDy.load     (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SampleHostInputs
//
//  Motion is recorded as the whole amount waiting, which a replay stores; the
//  drain in Tick records again only when more arrived after this.
//
////////////////////////////////////////////////////////////////////////////////

void AppleMouse::SampleHostInputs()
{
    bool      hasTarget = m_hasTarget.load  (std::memory_order_acquire);
    uint32_t  target    = m_hostTarget.load (std::memory_order_acquire);
    int       dx        = m_hostDx.load     (std::memory_order_acquire);
    int       dy        = m_hostDy.load     (std::memory_order_acquire);



    ObserveButton (m_hostButton.load (std::memory_order_acquire));

    if (hasTarget != m_observedHasTarget || target != m_observedTarget)
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::MouseTarget, hasTarget ? 1 : 0, 0, target);
        m_observedHasTarget = hasTarget;
        m_observedTarget    = target;
    }

    if (dx != m_observedDx || dy != m_observedDy)
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::MouseMove, 0, 0, PackMotion (dx, dy));
        m_observedDx = dx;
        m_observedDy = dy;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PackMotion
//
//  Both deltas in one record's data, X in the high half.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t AppleMouse::PackMotion (int dx, int dy)
{
    return (static_cast<uint64_t> (static_cast<uint32_t> (dx)) << 32) | static_cast<uint32_t> (dy);
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
    m_sampleAccum = 0;

    SyncObservedInputs();
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





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleMouse::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteUInt32 (static_cast<uint32_t> (m_hostDx.load (std::memory_order_acquire)));
    writer.WriteUInt32 (static_cast<uint32_t> (m_hostDy.load (std::memory_order_acquire)));
    writer.WriteBool   (m_hostButton.load (std::memory_order_acquire));
    writer.WriteUInt32 (m_hostTarget.load (std::memory_order_acquire));
    writer.WriteBool   (m_hasTarget.load  (std::memory_order_acquire));
    writer.WriteUInt32 (m_retargetCountdown);
    writer.WriteUInt32 (static_cast<uint32_t> (m_pendingX));
    writer.WriteUInt32 (static_cast<uint32_t> (m_pendingY));
    writer.WriteBool   (m_xInt);
    writer.WriteBool   (m_yInt);
    writer.WriteBool   (m_vblInt);
    writer.WriteByte   (m_mouX1);
    writer.WriteByte   (m_mouY1);
    writer.WriteBool   (m_xyEnabled);
    writer.WriteBool   (m_vblEnabled);
    writer.WriteBool   (m_x0EdgeFalling);
    writer.WriteBool   (m_y0EdgeFalling);
    writer.WriteBool   (m_iouAccessEnabled);
    writer.WriteBool   (m_lastInVblank);
    writer.WriteUInt32 (m_sampleAccum);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  The movement queue is range-checked against the clamp Tick applies, so a
//  loaded queue can never hold more than live motion could have built up.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleMouse::LoadState (StateReader & reader)
{
    constexpr int  kMaxPending = 1023;
    HRESULT        hr          = S_OK;
    uint16_t       version     = 0;
    uint32_t       hostDx      = 0;
    uint32_t       hostDy      = 0;
    bool           hostButton  = false;
    uint32_t       hostTarget  = 0;
    bool           hasTarget   = false;
    uint32_t       pendingX    = 0;
    uint32_t       pendingY    = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt32 (hostDx);
    reader.ReadUInt32 (hostDy);
    reader.ReadBool   (hostButton);
    reader.ReadUInt32 (hostTarget);
    reader.ReadBool   (hasTarget);
    reader.ReadUInt32 (m_retargetCountdown);
    reader.ReadUInt32 (pendingX);
    reader.ReadUInt32 (pendingY);
    reader.ReadBool   (m_xInt);
    reader.ReadBool   (m_yInt);
    reader.ReadBool   (m_vblInt);
    reader.ReadByte   (m_mouX1);
    reader.ReadByte   (m_mouY1);
    reader.ReadBool   (m_xyEnabled);
    reader.ReadBool   (m_vblEnabled);
    reader.ReadBool   (m_x0EdgeFalling);
    reader.ReadBool   (m_y0EdgeFalling);
    reader.ReadBool   (m_iouAccessEnabled);
    reader.ReadBool   (m_lastInVblank);
    reader.ReadUInt32 (m_sampleAccum);

    hr = reader.EndSection();
    CHR (hr);

    m_pendingX = static_cast<int> (pendingX);
    m_pendingY = static_cast<int> (pendingY);

    CBREx (m_pendingX >= -kMaxPending && m_pendingX <= kMaxPending, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (m_pendingY >= -kMaxPending && m_pendingY <= kMaxPending, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_hostDx.store     (static_cast<int> (hostDx), std::memory_order_release);
    m_hostDy.store     (static_cast<int> (hostDy), std::memory_order_release);
    m_hostButton.store (hostButton,                std::memory_order_release);
    m_hostTarget.store (hostTarget,                std::memory_order_release);
    m_hasTarget.store  (hasTarget,                 std::memory_order_release);

    SyncObservedInputs();
    UpdateIrqLines();

Error:
    return hr;
}
