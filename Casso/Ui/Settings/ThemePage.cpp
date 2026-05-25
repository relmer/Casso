#include "Pch.h"

#include "ThemePage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int  s_kRowHeightDp     = 28;
    constexpr int  s_kLabelWidthDp    = 140;
    constexpr int  s_kDropdownWidthDp = 220;
    constexpr int  s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::SetThemes
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::SetThemes (std::vector<std::string>  themeIds,
                           std::vector<std::wstring> displayNames,
                           int                       activeIndex)
{
    m_themeIds    = std::move (themeIds);
    m_activeIndex = activeIndex;

    if (displayNames.size() != m_themeIds.size())
    {
        // Caller mismatch -- fall back to ids as labels so the panel
        // is still functional even if a friendly name table is missing.
        displayNames.clear();
        for (const std::string & id : m_themeIds)
        {
            displayNames.emplace_back (id.begin(), id.end());
        }
    }

    m_themeDropdown.SetItems    (displayNames);
    m_themeDropdown.SetSelected (m_activeIndex);
    m_themeDropdown.SetSelect ([this] (int idx)
    {
        if (idx < 0 || idx >= (int) m_themeIds.size())
        {
            return;
        }
        m_activeIndex = idx;
        if (m_onThemeSelected)
        {
            m_onThemeSelected (m_themeIds[(size_t) idx]);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Layout (const RECT & rect, const DpiScaler & scaler)
{
    UINT  dpi        = scaler.Dpi();
    int   pad        = scaler.Px (s_kPagePadDp);
    int   rowHeight  = scaler.Px (s_kRowHeightDp);
    int   labelWidth = scaler.Px (s_kLabelWidthDp);
    int   dropWidth  = scaler.Px (s_kDropdownWidthDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   controlsX  = x + labelWidth;



    m_themeLabel.SetRect    (MakeRect (x, y, labelWidth, rowHeight));
    m_themeLabel.SetText    (L"Theme:");
    m_themeDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));

    m_themeLabel.SetDpi    (dpi);
    m_themeDropdown.SetDpi (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnLButtonDown (int x, int y)
{
    if (m_themeDropdown.OnLButtonDown (x, y)) { return; }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnLButtonUp (int x, int y)
{
    (void) m_themeDropdown.OnLButtonUp (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnMouseHover (int x, int y)
{
    m_themeDropdown.SetMouseHover (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool ThemePage::OnKey (WPARAM vk)
{
    if (m_themeDropdown.HandleKey (vk)) { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::CollectFocusables
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::CollectFocusables (std::vector<std::function<void (bool)>> & out)
{
    out.push_back ([this] (bool f) { m_themeDropdown.SetFocused (f); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    m_themeLabel.Paint          (painter, text);
    m_themeDropdown.PaintBase   (painter, text);
    m_themeDropdown.PaintMenu   (painter, text);
}
