#include "Pch.h"

#include "Widgets/DxuiCalendar.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiTheme.h"
#include "Core/DxuiUnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::GetDaysInMonth
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCalendar::GetDaysInMonth (int year, int month)
{
    using namespace std::chrono;

    return (int) (unsigned) (year_month_day_last (std::chrono::year (year) / std::chrono::month ((unsigned) month) / last).day());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::StepMonth
//
////////////////////////////////////////////////////////////////////////////////

DxuiCalendar::Day DxuiCalendar::StepMonth (const Day & shown, int delta)
{
    int  index = shown.year * 12 + (shown.month - 1) + delta;



    return Day { index / 12, index % 12 + 1, 1 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::GetGrid
//
////////////////////////////////////////////////////////////////////////////////

std::array<DxuiCalendar::Day, 42> DxuiCalendar::GetGrid (int year, int month)
{
    using namespace std::chrono;

    std::array<Day, 42>  grid  = {};
    sys_days             first = std::chrono::year (year) / std::chrono::month ((unsigned) month) / 1;
    sys_days             start = first - days (weekday (first).c_encoding());



    for (size_t i = 0; i < grid.size(); i++)
    {
        year_month_day  ymd (start + days ((int) i));

        grid[i] = Day { (int) ymd.year(), (int) (unsigned) ymd.month(), (int) (unsigned) ymd.day() };
    }

    return grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::HitTest
//
////////////////////////////////////////////////////////////////////////////////

DxuiCalendar::Hit DxuiCalendar::HitTest (int xDip, int yDip, int & outCell)
{
    int  gridTop = kPadDip + kHeaderDip + kWeekdayDip;
    int  column  = (xDip - kPadDip) / kCellDip;
    int  row     = (yDip - gridTop) / kCellDip;



    outCell = -1;

    if (yDip >= kPadDip && yDip < kPadDip + kHeaderDip)
    {
        if (xDip >= kPadDip && xDip < kPadDip + kCellDip)
        {
            return Hit::Previous;
        }

        if (xDip >= kWidthDip - kPadDip - kCellDip && xDip < kWidthDip - kPadDip)
        {
            return Hit::Next;
        }

        return Hit::None;
    }

    if (xDip < kPadDip || yDip < gridTop || column < 0 || column >= 7 || row < 0 || row >= 6)
    {
        return Hit::None;
    }

    outCell = row * 7 + column;

    return Hit::Cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::Show
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCalendar::Show (DxuiHwndSource & popupHost, const IDxuiTheme & theme, RECT anchorClientPx, const Day & chosen, PickFn onPick)
{
    HRESULT                    hr     = S_OK;
    DxuiPopupHost::ShowParams  params;
    POINT                      tl     = { anchorClientPx.left, anchorClientPx.top };
    POINT                      br     = { anchorClientPx.right, anchorClientPx.bottom };
    SYSTEMTIME                 now    = {};



    Close();

    GetLocalTime (&now);

    m_host    = &popupHost;
    m_theme   = &theme;
    m_onPick  = std::move (onPick);
    m_chosen  = chosen;
    m_today   = Day { now.wYear, now.wMonth, now.wDay };
    m_shown   = (chosen.year != 0) ? Day { chosen.year, chosen.month, 1 } : Day { m_today.year, m_today.month, 1 };
    m_hotCell = -1;
    m_hotHit  = Hit::None;
    m_dpi     = GetDpiForWindow (popupHost.GetHwnd());

    ClientToScreen (popupHost.GetHwnd(), &tl);
    ClientToScreen (popupHost.GetHwnd(), &br);

    m_popup = popupHost.AcquirePopup();
    BAIL_OUT_IF (m_popup == nullptr, S_OK);

    params.ownerHwnd        = popupHost.GetHwnd();
    params.anchorRectScreen = { tl.x, tl.y, br.x, br.y };
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::OnClickOutside;
    params.input            = DxuiPopupInput::Interactive;
    params.shadow           = true;
    params.sizeDip          = { kWidthDip, kHeightDip };
    params.backgroundArgb   = theme.BackgroundElevated();
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text) { Render (painter, text); };
    params.onMoveInside     = [this] (POINT localPx) { OnMove  (localPx); };
    params.onClickInside    = [this] (POINT localPx) { OnClick (localPx); };
    params.onClosed         = [this]() { Close(); };

    hr = m_popup->Show (std::move (params));

Error:
    if (FAILED (hr))
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::Close
//
//  m_popup is cleared before the release, so the popup's onClosed, which
//  comes back here, finds nothing to release.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCalendar::Close()
{
    DxuiPopupHost  * popup = m_popup;



    m_popup = nullptr;

    if (popup != nullptr && m_host != nullptr)
    {
        m_host->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::OnMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCalendar::OnMove (POINT localPx)
{
    int  cell = -1;
    Hit  hit  = HitTest (MulDiv (localPx.x, 96, (int) m_dpi), MulDiv (localPx.y, 96, (int) m_dpi), cell);



    if ((hit != m_hotHit || cell != m_hotCell) && m_popup != nullptr)
    {
        m_hotHit  = hit;
        m_hotCell = cell;
        m_popup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::OnClick
//
//  An arrow steps the month on display; a day in it or a neighboring month
//  is picked, and the calendar closes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCalendar::OnClick (POINT localPx)
{
    int     cell   = -1;
    Hit     hit    = HitTest (MulDiv (localPx.x, 96, (int) m_dpi), MulDiv (localPx.y, 96, (int) m_dpi), cell);
    PickFn  onPick = m_onPick;
    Day     picked;



    if (hit == Hit::Previous || hit == Hit::Next)
    {
        m_shown = StepMonth (m_shown, (hit == Hit::Previous) ? -1 : 1);

        if (m_popup != nullptr)
        {
            m_popup->MarkDirty();
        }

        return;
    }

    if (hit != Hit::Cell)
    {
        return;
    }

    picked = GetGrid (m_shown.year, m_shown.month)[(size_t) cell];

    if (m_popup != nullptr)
    {
        m_popup->Close (IDOK);
    }

    if (onPick)
    {
        onPick (picked);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar::Render
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCalendar::Render (IDxuiPainter & painter, IDxuiTextRenderer & text)
{
    static constexpr const wchar_t *  kMonths[]   = { L"January", L"February", L"March", L"April", L"May", L"June", L"July",
                                                      L"August", L"September", L"October", L"November", L"December" };
    static constexpr const wchar_t *  kWeekdays[] = { L"Su", L"Mo", L"Tu", L"We", L"Th", L"Fr", L"Sa" };
    float                             scale       = (float) m_dpi / 96.0f;
    float                             cell        = kCellDip * scale;
    float                             pad         = kPadDip * scale;
    float                             font        = 13.0f * scale;
    float                             gridTop     = (kPadDip + kHeaderDip + kWeekdayDip) * scale;
    std::array<Day, 42>               grid        = GetGrid (m_shown.year, m_shown.month);
    std::wstring                      title       = std::format (L"{} {}", kMonths[m_shown.month - 1], m_shown.year);
    HRESULT                           hr          = S_OK;



    if (m_theme == nullptr)
    {
        return;
    }

    //  The month and year, between the arrows that step it.
    for (Hit arrow : { Hit::Previous, Hit::Next })
    {
        float  x = (arrow == Hit::Previous) ? pad : (kWidthDip - kPadDip - kCellDip) * scale;

        if (m_hotHit == arrow)
        {
            painter.FillRoundedRect (x, pad, cell, kHeaderDip * scale, 4.0f * scale, m_theme->HoverBackground());
        }

        hr = text.DrawString ((arrow == Hit::Previous) ? s_kpszMdl2ChevronLeft : s_kpszMdl2ChevronRight, x, pad, cell, kHeaderDip * scale, m_theme->Foreground(),
                              font * 0.8f, kGlyphFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.DrawString (title.c_str(), pad + cell, pad, (kWidthDip - kPadDip * 2 - kCellDip * 2) * scale, kHeaderDip * scale, m_theme->Foreground(),
                          font * 1.1f, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int column = 0; column < 7; column++)
    {
        hr = text.DrawString (kWeekdays[column], pad + column * cell, (kPadDip + kHeaderDip) * scale, cell, kWeekdayDip * scale,
                              m_theme->ForegroundMuted(), font * 0.9f, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The days, the neighbors' muted, the chosen one filled and today
    //  outlined.
    for (size_t i = 0; i < grid.size(); i++)
    {
        const Day  & day    = grid[i];
        float        x      = pad + (float) (i % 7) * cell;
        float        y      = gridTop + (float) (i / 7) * cell;
        float        inset  = 2.0f * scale;
        bool         chosen = day.year == m_chosen.year && day.month == m_chosen.month && day.day == m_chosen.day;
        bool         today  = day.year == m_today.year && day.month == m_today.month && day.day == m_today.day;
        bool         inside = day.month == m_shown.month;
        uint32_t     ink    = chosen ? m_theme->ButtonText() : inside ? m_theme->Foreground() : m_theme->ForegroundDisabled();

        if (chosen)
        {
            painter.FillRoundedRect (x + inset, y + inset, cell - inset * 2, cell - inset * 2, (cell - inset * 2) / 2, m_theme->Accent());
            ink = 0xFFFFFFFF;
        }
        else if ((int) i == m_hotCell)
        {
            painter.FillRoundedRect (x + inset, y + inset, cell - inset * 2, cell - inset * 2, (cell - inset * 2) / 2, m_theme->HoverBackground());
        }

        if (today && !chosen)
        {
            painter.OutlineRoundedRect (x + inset, y + inset, cell - inset * 2, cell - inset * 2, (cell - inset * 2) / 2, scale, m_theme->Accent());
        }

        hr = text.DrawString (std::to_wstring (day.day).c_str(), x, y, cell, cell, ink, font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
