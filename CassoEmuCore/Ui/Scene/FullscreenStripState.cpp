#include "Pch.h"

#include "Ui/Scene/FullscreenStripState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState::Tick
//
//  Transition order matters: the hotkey outranks the pointer (it is the only
//  summon while the guest owns the pointer), pinning outranks auto-hide, and
//  the capture restore fires only on the Hiding -> Hidden edge so it cannot
//  double-fire.
//
////////////////////////////////////////////////////////////////////////////////

StripEffects FullscreenStripState::Tick (const StripInputs & inputs)
{
    StripEffects  effects;
    bool          guestOwns = inputs.guestPointer != GuestPointerMode::None;
    bool          engaged   = inputs.pointerOverStrip || inputs.pinned;



    if (engaged)
    {
        m_lastEngagedMs = inputs.nowMs;
    }

    // Hotkey: toggles. Summoning under guest capture releases it and records
    // what to restore; dismissing lets the normal hide path run (the restore
    // fires when Hidden is reached).
    if (inputs.hotkey)
    {
        if (m_mode == StripMode::Hidden || m_mode == StripMode::Hiding)
        {
            if (guestOwns && !m_summonedByHotkey)
            {
                effects.releaseCapture = true;
                m_summonedByHotkey     = true;
                m_capturedAtSummon     = inputs.guestPointer;
            }

            BeginReveal (inputs.nowMs);
        }
        else if (!inputs.pinned)
        {
            BeginHide (inputs.nowMs);
        }
    }

    // Edge reveal: host-owned pointer only -- while the guest owns it, edge
    // proximity is guest input, not a summon.
    if (m_mode == StripMode::Hidden && inputs.pointerAtBottomEdge && !guestOwns)
    {
        BeginReveal (inputs.nowMs);
    }

    switch (m_mode)
    {
        case StripMode::Revealing:
            if (inputs.nowMs - m_animStartMs >= kSlideMs)
            {
                m_mode          = StripMode::Shown;
                m_lastEngagedMs = inputs.nowMs;   // fresh grace on arrival
            }

            break;

        case StripMode::Shown:
            if (!engaged && inputs.nowMs - m_lastEngagedMs >= kAutoHideGraceMs)
            {
                BeginHide (inputs.nowMs);
            }

            break;

        case StripMode::Hiding:
            // Re-engagement (pointer back, or a tooltip/browse pin) reverses
            // the slide; pinning must never let Hidden be reached.
            if (engaged)
            {
                BeginReveal (inputs.nowMs);
            }
            else if (inputs.nowMs - m_animStartMs >= kSlideMs)
            {
                m_mode = StripMode::Hidden;

                // The exactly-once restore: only the Hiding -> Hidden edge
                // emits it, and the summon record clears with it.
                if (m_summonedByHotkey)
                {
                    effects.restoreCapture = m_capturedAtSummon;
                    m_summonedByHotkey     = false;
                    m_capturedAtSummon     = GuestPointerMode::None;
                }
            }

            break;

        case StripMode::Hidden:
        default:
            break;
    }

    m_indicator = (m_mode == StripMode::Hidden) && inputs.anyDriveActive;

    return effects;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState::SlideProgress
//
////////////////////////////////////////////////////////////////////////////////

float FullscreenStripState::SlideProgress (int64_t nowMs) const
{
    float  t = (float) (nowMs - m_animStartMs) / (float) kSlideMs;



    t = std::clamp (t, 0.0f, 1.0f);

    switch (m_mode)
    {
        case StripMode::Revealing:  return t;
        case StripMode::Hiding:     return 1.0f - t;
        case StripMode::Shown:      return 1.0f;
        case StripMode::Hidden:
        default:                    return 0.0f;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState::BeginReveal
//
////////////////////////////////////////////////////////////////////////////////

void FullscreenStripState::BeginReveal (int64_t nowMs)
{
    m_animStartMs   = (m_mode == StripMode::Hiding) ? MirroredStart (nowMs) : nowMs;
    m_mode          = StripMode::Revealing;
    m_lastEngagedMs = nowMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState::BeginHide
//
////////////////////////////////////////////////////////////////////////////////

void FullscreenStripState::BeginHide (int64_t nowMs)
{
    m_animStartMs = (m_mode == StripMode::Revealing) ? MirroredStart (nowMs) : nowMs;
    m_mode        = StripMode::Hiding;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState::MirroredStart
//
////////////////////////////////////////////////////////////////////////////////

int64_t FullscreenStripState::MirroredStart (int64_t nowMs) const
{
    int64_t  elapsed = std::clamp (nowMs - m_animStartMs, (int64_t) 0, kSlideMs);



    return nowMs - (kSlideMs - elapsed);
}
