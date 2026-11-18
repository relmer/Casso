#pragma once

#include "Pch.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendar
//
//  A month's days in a popup under a field, as Windows' date picker drops
//  its calendar: the month and year between arrows that step a month, the
//  weekday letters, and six weeks of days, with today outlined and the
//  chosen day filled. A click on a day picks it and closes the popup.
//
//  Separate from any field, so it is testable: the grid and the hit test are
//  plain functions of the month on display.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCalendar
{
public:
    struct Day
    {
        int  year  = 0;
        int  month = 0;   // 1 to 12
        int  day   = 0;   // 1 to 31
    };

    using PickFn = std::function<void (const Day & day)>;

    //  Opens under `anchorClientPx`, in the popup host's client pixels, on
    //  the month of `chosen`, or of today when `chosen` has no year.
    void  Show   (DxuiHwndSource & popupHost, const IDxuiTheme & theme, RECT anchorClientPx, const Day & chosen, PickFn onPick);
    void  Close  ();
    bool  IsOpen () const { return m_popup != nullptr; }

    //  The 42 days in the grid for a month, from the Sunday on or before
    //  its first day.
    static std::array<Day, 42>  GetGrid        (int year, int month);
    static int                  GetDaysInMonth (int year, int month);
    static Day                  StepMonth      (const Day & shown, int delta);

    //  What a point in the popup is on, in DIPs from its top left: an arrow,
    //  a cell of the grid, or nothing.
    enum class Hit { None, Previous, Next, Cell };

    static Hit  HitTest (int xDip, int yDip, int & outCell);

    static constexpr const wchar_t *  kGlyphFace = L"Segoe MDL2 Assets";

    static constexpr int  kCellDip    = 36;
    static constexpr int  kHeaderDip  = 40;
    static constexpr int  kWeekdayDip = 28;
    static constexpr int  kPadDip     = 8;
    static constexpr int  kWidthDip   = kPadDip * 2 + kCellDip * 7;
    static constexpr int  kHeightDip  = kPadDip * 2 + kHeaderDip + kWeekdayDip + kCellDip * 6;

private:
    void  Render  (IDxuiPainter & painter, IDxuiTextRenderer & text);
    void  OnMove  (POINT localPx);
    void  OnClick (POINT localPx);

    DxuiHwndSource     * m_host     = nullptr;
    DxuiPopupHost      * m_popup    = nullptr;
    const IDxuiTheme   * m_theme    = nullptr;
    Day                  m_shown;
    Day                  m_chosen;
    Day                  m_today;
    int                  m_hotCell  = -1;
    Hit                  m_hotHit   = Hit::None;
    UINT                 m_dpi      = 96;
    PickFn               m_onPick;
};
