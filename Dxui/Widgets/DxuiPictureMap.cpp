#include "Pch.h"

#include "DxuiPictureMap.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::Begin
//
//  The zoom and origin start over for a picture, the whole picture showing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::Begin (const Key & key)
{
    m_key     = key;
    m_zoom    = 1.0f;
    m_originX = 0.0f;
    m_originY = 0.0f;
    m_panning = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::ClampOrigin
//
//  Keeps the zoomed view inside the picture: the origin no farther right
//  or down than leaves the view's width and height of picture showing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::ClampOrigin()
{
    float  shownWidth  = (float) m_key.width  / m_zoom;
    float  shownHeight = (float) m_key.height / m_zoom;



    m_originX = (std::clamp) (m_originX, 0.0f, (std::max) (0.0f, (float) m_key.width  - shownWidth));
    m_originY = (std::clamp) (m_originY, 0.0f, (std::max) (0.0f, (float) m_key.height - shownHeight));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::Zoom
//
//  Zooms around the pointer, so the picture pixel under it stays under it,
//  by 1.25 times a notch, from the whole picture to kMaxZoom times.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::Zoom (const Key & key, const Frame & frame, POINT point, float notches)
{
    constexpr float  kStep  = 1.25f;
    float            pointX = 0.0f;
    float            pointY = 0.0f;



    if (!IsFor (key))
    {
        Begin (key);
    }

    pointX    = ((float) point.x - frame.left) / (frame.scale * m_zoom) + m_originX;
    pointY    = ((float) point.y - frame.top)  / (frame.scale * m_zoom) + m_originY;
    m_zoom    = (std::clamp) (m_zoom * std::pow (kStep, notches), 1.0f, (float) kMaxZoom);
    m_originX = pointX - ((float) point.x - frame.left) / (frame.scale * m_zoom);
    m_originY = pointY - ((float) point.y - frame.top)  / (frame.scale * m_zoom);

    ClampOrigin();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::StartPan
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::StartPan (const Key & key, POINT point)
{
    if (!IsFor (key))
    {
        Begin (key);
    }

    m_panning    = true;
    m_panFrom    = point;
    m_panOriginX = m_originX;
    m_panOriginY = m_originY;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::Pan
//
//  The picture follows the pointer: the origin moves back by the pointer's
//  travel since the press, in the picture's pixels at its zoom.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::Pan (const Frame & frame, POINT point)
{
    if (!m_panning || frame.scale <= 0.0f)
    {
        return;
    }

    m_originX = m_panOriginX - (float) (point.x - m_panFrom.x) / (frame.scale * m_zoom);
    m_originY = m_panOriginY - (float) (point.y - m_panFrom.y) / (frame.scale * m_zoom);

    ClampOrigin();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::TickPing
//
//  The ping starts on the first tick a pinging picture is shown. A frame
//  changes while its rings are spreading, and once more as they end, so the
//  last one is erased.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPictureMap::TickPing (int64_t nowMs, bool isPinging)
{
    bool  isRunning = false;
    bool  changed   = false;



    if (!isPinging)
    {
        changed       = m_pingDrawn;
        m_pingEpochMs = -1;
        m_pingDrawn   = false;
        return changed;
    }

    if (m_pingEpochMs < 0)
    {
        m_pingEpochMs = nowMs;
    }

    isRunning   = (nowMs - m_pingEpochMs) % kPingPeriodMs < kPingRunMs;
    changed     = isRunning || m_pingDrawn;
    m_pingDrawn = isRunning;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMap::PaintPing
//
//  Rings spreading from the ping point, like ripples from a stone dropped
//  in a pond: kPingRings of them, a little apart, each growing to
//  kPingRadiusDip and fading as it grows, in the first kPingRunMs of every
//  kPingPeriodMs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPictureMap::PaintPing (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler,
                                int pingX, int pingY, float left, float top, float scale) const
{
    constexpr float     kRingSpacing = 0.22f;
    constexpr float     kAlphaFull   = 255.0f;
    constexpr int       kAlphaShift  = 24;
    constexpr uint32_t  kRgbMask     = 0x00FFFFFFu;
    int64_t             phase        = 0;
    float               run          = 0.0f;
    float               cx           = left + ((float) pingX + 0.5f) * scale;
    float               cy           = top  + ((float) pingY + 0.5f) * scale;
    float               radius       = scaler.ToPxf ((float) kPingRadiusDip);
    float               stroke       = scaler.ToPxf (2.0f);
    uint32_t            rgb          = theme.Accent() & kRgbMask;



    if (pingX < 0 || pingY < 0 || m_pingEpochMs < 0)
    {
        return;
    }

    phase = ((int64_t) GetTickCount64() - m_pingEpochMs) % kPingPeriodMs;

    if (phase >= kPingRunMs)
    {
        return;
    }

    run = (float) phase / (float) kPingRunMs;

    for (int i = 0; i < kPingRings; i++)
    {
        float     t     = (run - (float) i * kRingSpacing) / (1.0f - (float) (kPingRings - 1) * kRingSpacing);
        float     fade  = 1.0f - t;
        uint32_t  alpha = (uint32_t) (kAlphaFull * fade * fade);
        HRESULT   hr    = S_OK;

        if (t <= 0.0f || t >= 1.0f)
        {
            continue;
        }

        hr = text.DrawEllipse (cx, cy, radius * t, radius * t, stroke, (alpha << kAlphaShift) | rgb);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
