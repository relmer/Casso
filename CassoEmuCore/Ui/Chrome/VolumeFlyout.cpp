#include "Pch.h"

#include "VolumeFlyout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFlyout::VolumeFlyout
//
//  The readout names the state, not just the number: "Muted" while muted
//  (the slider shows 0 then), the percentage otherwise.
//
////////////////////////////////////////////////////////////////////////////////

VolumeFlyout::VolumeFlyout()
{
    m_focusable = false;

    m_slider.SetVertical      (true);
    m_slider.SetRange         (0.0f, 100.0f);
    m_slider.SetStep          (1.0f);
    m_slider.SetSuffix        (L"%");
    m_slider.SetDecimalPlaces (0);
    m_slider.SetShowTicks     (false);
    m_slider.SetValue         (100.0f);

    m_slider.SetOnChange ([this] (float v)
    {
        m_volume01 = v / 100.0f;
        if (m_sink) { m_sink (m_volume01, m_muted); }
    });

    m_slider.SetValueFormatter ([this] (float v) -> std::wstring
    {
        wchar_t  buf[16] = {};

        if (m_muted)
        {
            return L"Muted";
        }

        swprintf_s (buf, L"%d%%", (int) std::lround (v));
        return buf;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFlyout::SetVolume / ToggleMute
//
//  Muted DISPLAYS as silence -- slider at the bottom, 0% -- while the stored
//  level survives underneath, so unmuting restores exactly what the user
//  had. The slider is disabled while muted, so the zeroed display can never
//  be dragged into becoming the stored value.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeFlyout::SetVolume (float volume01, bool muted)
{
    m_volume01 = std::clamp (volume01, 0.0f, 1.0f);
    m_muted    = muted;

    m_slider.SetValue   (m_muted ? 0.0f : m_volume01 * 100.0f);
    m_slider.SetEnabled (!m_muted);
}


void VolumeFlyout::ToggleMute()
{
    SetVolume (m_volume01, !m_muted);

    if (m_sink) { m_sink (m_volume01, m_muted); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFlyout::Layout / Paint / OnMouse
//
//  The slider gets first claim on motion while it is tracking a drag, and
//  only starts one while UNMUTED.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeFlyout::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_slider.SetRect (boundsDip);
    m_slider.SetDpi  (scaler.GetDpi());
    SetBounds (boundsDip);
}


void VolumeFlyout::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_slider.SetEnabled (!m_muted);
    m_slider.Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFlyout::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeFlyout::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = m_slider.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        m_slider.SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Down:
        handled = !m_muted && m_slider.OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Up:
        handled = m_slider.OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        break;

    default:
        break;
    }

    return handled;
}