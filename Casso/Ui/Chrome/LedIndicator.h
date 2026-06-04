#pragma once

#include "Pch.h"

#include "ChromeTheme.h"





enum class LedState
{
    Idle,
    Present,
    Active,
};


struct LedIndicatorLayout
{
    RECT  haloRect = {};
    RECT  coreRect = {};
};


class LedIndicator
{
public:
    void               Layout       (int x, int y, UINT dpi);
    void               SetState     (LedState state) { m_state = state; }
    LedState           GetState     () const { return m_state; }
    LedIndicatorLayout GetLayout    () const { return m_layout; }
    uint32_t           CoreArgb     (const ChromeTheme & theme) const;
    uint32_t           HaloArgb     (const ChromeTheme & theme) const;
    void               Paint        (DxuiPainter & painter, const ChromeTheme & theme) const;
    void               Paint        (DxuiPainter & painter, uint32_t coreArgb, uint32_t haloArgb) const;

private:
    LedState            m_state  = LedState::Idle;
    LedIndicatorLayout  m_layout = {};
};
