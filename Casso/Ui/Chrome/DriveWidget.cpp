#include "Pch.h"

#include "DriveWidget.h"

#include "../IDriveCommandSink.h"





namespace
{
    constexpr int    s_kBaseDpi          = 96;
    constexpr int    s_kBodyWidthPx      = 190;
    constexpr int    s_kBodyHeightPx     = 44;
    constexpr int    s_kSlotInsetPx      = 8;
    constexpr int    s_kSlotHeightPx     = 12;
    constexpr int    s_kEjectSizePx      = 14;
    constexpr int    s_kLabelPadPx       = 8;
    constexpr float  s_kLabelFontDip     = 12.0f;
    constexpr int    s_kSpinDotCount     = 10;
    constexpr int    s_kSpinRadiusPx     = 10;
    constexpr wchar_t s_kFontFamily[]    = L"Segoe UI";


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }


    int Scale (int value, UINT dpi)
    {
        UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;



        return MulDiv (value, (int) effectiveDpi, s_kBaseDpi);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Initialize (int slot, int drive, IDriveCommandSink * pSink)
{
    m_slot  = slot;
    m_drive = drive;
    m_sink  = pSink;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Layout (int x, int y, UINT dpi)
{
    int  bodyW      = Scale (s_kBodyWidthPx, dpi);
    int  bodyH      = Scale (s_kBodyHeightPx, dpi);
    int  slotInset  = Scale (s_kSlotInsetPx, dpi);
    int  slotH      = Scale (s_kSlotHeightPx, dpi);
    int  ejectSize  = Scale (s_kEjectSizePx, dpi);
    int  labelPad   = Scale (s_kLabelPadPx, dpi);



    m_bodyRect.left   = x;
    m_bodyRect.top    = y;
    m_bodyRect.right  = x + bodyW;
    m_bodyRect.bottom = y + bodyH;

    m_slotRect.left   = x + slotInset;
    m_slotRect.top    = y + slotInset;
    m_slotRect.right  = x + bodyW - slotInset;
    m_slotRect.bottom = m_slotRect.top + slotH;

    m_ejectRect.left   = x + labelPad;
    m_ejectRect.bottom = y + bodyH - labelPad;
    m_ejectRect.right  = m_ejectRect.left + ejectSize;
    m_ejectRect.top    = m_ejectRect.bottom - ejectSize;

    m_led.Layout (m_bodyRect.right - labelPad - ejectSize,
                  m_bodyRect.bottom - labelPad - ejectSize,
                  dpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SyncFromState
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::SyncFromState (const DriveWidgetState & state)
{
    bool  mounted = state.IsMounted();
    bool  motorOn = state.motorOn.load (std::memory_order_relaxed);
    bool  active  = motorOn || state.diskActive.load (std::memory_order_relaxed);



    m_state.mountedImagePath      = state.mountedImagePath;
    m_state.doorState             = state.doorState;
    m_state.animationStartTimeMs  = state.animationStartTimeMs;
    m_state.lastSyncEventId       = state.lastSyncEventId;
    m_state.motorOn.store (motorOn, std::memory_order_relaxed);
    m_state.diskActive.store (active, std::memory_order_relaxed);

    if (active)
    {
        m_led.SetState (LedState::Active);
    }
    else if (mounted)
    {
        m_led.SetState (LedState::Present);
    }
    else
    {
        m_led.SetState (LedState::Idle);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Paint (
    DxUiPainter               & painter,
    DwriteTextRenderer        & text,
    const ChromeVisualState   & visual,
    const ChromeTheme         & theme)
{
    HRESULT  hr       = S_OK;
    int      bodyW    = m_bodyRect.right - m_bodyRect.left;
    int      bodyH    = m_bodyRect.bottom - m_bodyRect.top;
    int      slotW    = m_slotRect.right - m_slotRect.left;
    int      slotH    = m_slotRect.bottom - m_slotRect.top;
    int      doorH    = slotH;
    bool     motorOn  = m_state.motorOn.load (std::memory_order_relaxed);
    wchar_t  label[32] = {};



    painter.FillRect ((float) m_bodyRect.left, (float) m_bodyRect.top, (float) bodyW, (float) bodyH, theme.driveBodyArgb);
    painter.OutlineRect ((float) m_bodyRect.left, (float) m_bodyRect.top, (float) bodyW, (float) bodyH, 2.0f, theme.driveBezelArgb);

    if (m_state.doorState == DriveWidgetState::Door::Open || m_state.doorState == DriveWidgetState::Door::Opening)
    {
        doorH = slotH / 2;
    }

    painter.FillRect ((float) m_slotRect.left, (float) m_slotRect.top, (float) slotW, (float) slotH, 0xFF101010);
    painter.FillRect ((float) m_slotRect.left, (float) m_slotRect.top, (float) slotW, (float) doorH, theme.driveBezelArgb);
    painter.FillRect ((float) m_ejectRect.left,
                      (float) m_ejectRect.top,
                      (float) (m_ejectRect.right - m_ejectRect.left),
                      (float) (m_ejectRect.bottom - m_ejectRect.top),
                      0xFF6C604B);

    if (motorOn)
    {
        for (int i = 0; i < s_kSpinDotCount; i++)
        {
            float  angle = ((float) (i + visual.frameIndex) * 6.2831853f) / (float) s_kSpinDotCount;
            float  cx    = (float) (m_bodyRect.left + bodyW / 2) + cosf (angle) * (float) s_kSpinRadiusPx;
            float  cy    = (float) (m_bodyRect.top + bodyH / 2) + sinf (angle) * (float) s_kSpinRadiusPx;
            painter.FillRect (cx, cy, 4.0f, 4.0f, 0xAA202020);
        }
    }

    m_led.Paint (painter, theme);

    swprintf_s (label, L"DRIVE %d", m_drive + 1);
    IGNORE_RETURN_VALUE (hr, text.DrawString (label,
                                              (float) (m_bodyRect.left + s_kLabelPadPx),
                                              (float) (m_bodyRect.bottom - s_kLabelPadPx - 22),
                                              (float) bodyW,
                                              20.0f,
                                              theme.driveLabelArgb,
                                              s_kLabelFontDip,
                                              s_kFontFamily));
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetRegion DriveWidget::HitTest (int x, int y) const
{
    if (RectContains (m_ejectRect, x, y))
    {
        return DriveWidgetRegion::Eject;
    }

    if (RectContains (m_bodyRect, x, y))
    {
        return DriveWidgetRegion::Body;
    }

    return DriveWidgetRegion::None;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnDrop
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DriveWidget::OnDrop (const std::wstring & path)
{
    HRESULT  hr = S_OK;



    CBRA (m_sink);

    hr = m_sink->Mount (m_slot, m_drive, path);
    CHR (hr);

Error:
    return hr;
}
