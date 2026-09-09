#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState
//
//  The fullscreen drive overlay strip's state machine: edge-reveal (only while
//  the host owns the pointer), hotkey summon with guest-capture release and
//  restore, auto-hide with tooltip/browse pinning, and the hidden-state
//  activity indicator. A pure FSM -- inputs in, effects out -- so the capture
//  sequencing rules are property-testable.
//
//  Invariants (the data-model's four, enforced here and property-tested):
//    - edge-reveal never fires while the guest owns the pointer; only the
//      hotkey summons then, releasing the capture for the interaction
//    - Hidden is never entered while pinned (tooltip visible or a browse
//      dialog open from the strip)
//    - a capture released at summon is restored EXACTLY once, when the strip
//      finishes hiding
//    - the activity indicator shows only while Hidden
//
////////////////////////////////////////////////////////////////////////////////

enum class StripMode
{
    Hidden,
    Revealing,
    Shown,
    Hiding,
};


enum class GuestPointerMode
{
    None,
    Mouse,
    Paddle,
};


//
//  One tick's worth of observations, sampled by the shell.
//
struct StripInputs
{
    int64_t           nowMs               = 0;
    bool              pointerAtBottomEdge = false;   // dwell zone along the screen bottom
    bool              pointerOverStrip    = false;
    bool              hotkey              = false;   // edge-triggered: one true per press
    bool              pinned              = false;   // tooltip visible or strip-opened browse dialog up
    GuestPointerMode  guestPointer        = GuestPointerMode::None;
    bool              anyDriveActive      = false;
};


//
//  Side effects the shell must perform after a tick. `restoreCapture` fires
//  exactly once per hotkey summon that released a capture.
//
struct StripEffects
{
    bool              releaseCapture = false;
    GuestPointerMode  restoreCapture = GuestPointerMode::None;
};


class FullscreenStripState
{
public:
    StripEffects  Tick (const StripInputs & inputs);

    StripMode  Mode              () const { return m_mode; }
    bool       ActivityIndicator () const { return m_indicator; }

    // 0 = fully hidden .. 1 = fully shown, sliding linearly through the
    // Revealing / Hiding animations.
    float      SlideProgress (int64_t nowMs) const;

    // Slide animation length and the pointer-leave grace before auto-hide.
    static constexpr int64_t  kSlideMs         = 180;
    static constexpr int64_t  kAutoHideGraceMs = 700;

private:
    void  BeginReveal (int64_t nowMs);
    void  BeginHide   (int64_t nowMs);

    // Progress-preserving transition: reversing mid-slide keeps the strip's
    // current position instead of snapping to the far end.
    int64_t  MirroredStart (int64_t nowMs) const;

    StripMode         m_mode             = StripMode::Hidden;
    int64_t           m_animStartMs      = 0;
    int64_t           m_lastEngagedMs    = 0;       // last over-strip / pinned sighting
    bool              m_summonedByHotkey = false;
    GuestPointerMode  m_capturedAtSummon = GuestPointerMode::None;
    bool              m_indicator        = false;
};
