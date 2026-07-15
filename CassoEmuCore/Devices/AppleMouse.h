#pragma once

#include "Pch.h"

#include "Core/ICycleSink.h"
#include "Core/IInterruptController.h"

class IVideoTiming;
class MemoryBus;




////////////////////////////////////////////////////////////////////////////////
//
//  AppleMouse
//
//  The Apple //c IOU mouse hardware: X0/Y0 movement-interrupt latches with
//  direction lines, the mouse button, the VBL interrupt latch, and the IOU
//  soft-switch programming interface. The //c mouse is built in (no card);
//  the real ROM 4 mouse firmware runs unmodified against this hardware
//  model — the register contract below was derived by disassembling that
//  firmware:
//
//      $C015 R7   X0-interrupt pending (read-only status)
//      $C017 R7   Y0-interrupt pending (read-only status)
//      $C019 R7   VBL-interrupt latch (latched at VBL onset; cleared by $C070)
//      $C048 R/W  RSTXY — clear both X0/Y0 interrupt latches
//      $C058-5F   IOU programming (only while IOU access is enabled):
//                   $C058 DISXY   $C059 ENBXY    (mask/enable X0/Y0 IRQ)
//                   $C05A DISVBL  $C05B ENVBL    (mask/enable VBL IRQ)
//                   $C05C/D X0EDGE, $C05E/F Y0EDGE (edge select; stored)
//      $C063 R7   mouse button, ACTIVE LOW (0 = pressed)
//      $C066 R7   MOUX1 X direction line: 1 = +X (firmware increments X)
//      $C067 R7   MOUY1 Y direction line: 0 = +Y (firmware inverts + tests)
//      $C070 R    PTRIG side effect: clears the VBL interrupt latch
//      $C078 W    IOU access off (default; $C058-5F = annunciator/DHIRES)
//      $C079 W    IOU access on  ($C058-5F = the mouse switches above)
//
//  Interrupt model: host motion accumulates as signed deltas; one unit per
//  axis latches at a time (direction line + pending flag), the aggregate
//  IRQ line is level-held while `enabled && latched`, and the $C048 ack
//  clears the latches so the next unit can load on a later Tick. This is
//  the "assert once per movement, hold until acknowledge, never re-fire
//  before the next movement" contract the firmware's service loop expects
//  (it steps position exactly ±1 per latched axis per interrupt).
//
//  Not a bus MemoryDevice: its registers interleave the keyboard's
//  $C000-$C063 range and the soft-switch bank's $C064-$C07F page, so those
//  devices forward to it (the $C028 ROM-bank forwarding precedent).
//  Host-side setters are thread-safe (UI thread); everything else runs on
//  the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class AppleMouse : public ICycleSink
{
public:
    AppleMouse() = default;

    // ---- Wiring ----------------------------------------------------------

    // Register the two IRQ sources (movement + VBL) with the shared
    // controller. Caller-owned; the controller outlives the device.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // VBL-onset source for the VBL interrupt latch. Caller-owned.
    void    SetVideoTiming (IVideoTiming * vt) { m_videoTiming = vt; }

    // Memory bus for reading the mouse firmware's screen holes (position +
    // clamp window) during Tick — CPU thread, so the reads are race-free
    // with guest execution and see the live MMU mapping (the shell's UI
    // thread must NOT read guest memory directly). Caller-owned.
    void    SetBus (MemoryBus * bus) { m_bus = bus; }

    // ---- Host input (any thread) -----------------------------------------

    void    MoveBy    (int dx, int dy);
    void    SetButton (bool down) { m_hostButton.store (down, std::memory_order_release); }

    // Absolute host->guest targeting: the host pointer's position
    // over the emulator viewport as 16-bit fractions (0..65535 across each
    // axis). Tick (CPU thread) projects the fraction into the firmware's
    // LIVE clamp window (read from the slot-7 screen holes via the bus) and
    // queues the delta from the firmware's current position as movement
    // units — self-correcting: units the firmware clamps away re-derive on
    // the next tick. Inert until the guest initializes the mouse firmware
    // (hole sanity checks) or when no bus is wired.
    void    SetHostTargetFraction (uint16_t fx, uint16_t fy);
    void    ClearHostTarget       () { m_hasTarget.store (false, std::memory_order_release); }

    // ---- ICycleSink (CPU thread, from EmuCpu::AddCycles) -------------------

    void    Tick (uint32_t cpuCycles) override;

    // ---- Soft-switch surface (CPU thread, forwarded by keyboard/bank) -----

    Byte    ReadXInterruptStatus() const { return m_xInt   ? 0x80 : 0x00; }   // $C015 bit 7
    Byte    ReadYInterruptStatus() const { return m_yInt   ? 0x80 : 0x00; }   // $C017 bit 7
    Byte    ReadVblInterrupt     () const { return m_vblInt ? 0x80 : 0x00; }   // $C019 bit 7
    Byte    ReadButton           () const;                                      // $C063 bit 7
    Byte    ReadMouX1            () const { return m_mouX1; }                   // $C066 bit 7
    Byte    ReadMouY1            () const { return m_mouY1; }                   // $C067 bit 7

    void    AccessRstXY   ();                          // $C048 any access
    void    AccessPtrig   ();                          // $C070 read side effect
    void    WriteIouAccess (bool enabled) { m_iouAccessEnabled = enabled; }   // $C079/$C078
    bool    IsIouAccessEnabled() const   { return m_iouAccessEnabled; }
    void    AccessIouSwitch (Word address);            // $C058-$C05F while enabled

    // ---- Lifecycle ---------------------------------------------------------

    void    Reset();

    // ---- Inspectors (tests) ------------------------------------------------

    bool    XyInterruptsEnabled  () const { return m_xyEnabled; }
    bool    VblInterruptsEnabled() const { return m_vblEnabled; }

private:
    void    UpdateIrqLines();
    void    RetargetFromHoles();

    // Firmware screen holes (slot 7): position, clamp min/max, per axis.
    static constexpr Word     kHoleXPosLo  = 0x047F, kHoleXPosHi  = 0x057F;
    static constexpr Word     kHoleYPosLo  = 0x04FF, kHoleYPosHi  = 0x05FF;
    static constexpr Word     kHoleXMinLo  = 0x047D, kHoleXMinHi  = 0x057D;
    static constexpr Word     kHoleXMaxLo  = 0x067D, kHoleXMaxHi  = 0x077D;
    static constexpr Word     kHoleYMinLo  = 0x04FD, kHoleYMinHi  = 0x05FD;
    static constexpr Word     kHoleYMaxLo  = 0x06FD, kHoleYMaxHi  = 0x07FD;

    // Retarget cadence: re-derive pending motion from the holes at ~500 Hz
    // rather than per instruction (12 bus reads per pass).
    static constexpr uint32_t kRetargetIntervalCycles = 2048;

    // Host-thread accumulator (drained by Tick on the CPU thread).
    std::atomic<int>          m_hostDx      { 0 };
    std::atomic<int>          m_hostDy      { 0 };
    std::atomic<bool>         m_hostButton  { false };

    // Absolute target: packed fx<<16|fy viewport fractions + validity flag.
    std::atomic<uint32_t>     m_hostTarget  { 0 };
    std::atomic<bool>         m_hasTarget   { false };
    uint32_t                  m_retargetCountdown = 0;
    class MemoryBus *         m_bus         = nullptr;

    // CPU-side movement queue: signed units not yet latched.
    int                       m_pendingX    = 0;
    int                       m_pendingY    = 0;

    // Interrupt latches + direction lines.
    bool                      m_xInt        = false;
    bool                      m_yInt        = false;
    bool                      m_vblInt      = false;
    Byte                      m_mouX1       = 0;
    Byte                      m_mouY1       = 0;

    // IOU programming state.
    bool                      m_xyEnabled        = false;
    bool                      m_vblEnabled       = false;
    bool                      m_x0EdgeFalling    = false;
    bool                      m_y0EdgeFalling    = false;
    bool                      m_iouAccessEnabled = false;

    // VBL edge detection.
    IVideoTiming *            m_videoTiming = nullptr;
    bool                      m_lastInVblank = false;

    IInterruptController *    m_ic          = nullptr;
    IrqSourceId               m_xySource    = 0;
    IrqSourceId               m_vblSource   = 0;
    bool                      m_irqBound    = false;
};
