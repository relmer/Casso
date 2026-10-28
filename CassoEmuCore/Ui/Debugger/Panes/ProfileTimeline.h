#pragma once

#include "Pch.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline
//
//  The profile's frames across a range of cycles, each a bar as wide as the
//  frames it covers and as tall as its share of their cycles, stacked by
//  routine in the
//  flame graph's colors for the kColoredRoutines hottest routines, the rest
//  of its cycles together in the divider color. A frame's tip lists its
//  routines. A line is drawn down the timeline at the pointer, in the
//  history timeline's color for its line at the pointer.
//
//  Bounds are pixels, like every Dxui control.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileTimeline : public IDxuiControl
{
public:
    static constexpr int     kHeightDip       = 40;
    static constexpr size_t  kColoredRoutines = 8;

    void  SetData (const std::optional<ProfileData> & profile);

    //  Where a frame's bar is, as an index in the frames, or none.
    std::optional<size_t>  FindFrameAt (POINT point) const;

    //  The cycles across the timeline: by default from the newest frame's
    //  end back RoutineProfile::kFrameCount frames, or further to the first
    //  frame, so a few frames sit at the right as the newest of a run do;
    //  zoomed, the range passed to SetView. A cycle can be before the first.
    using Range = std::pair<int64_t, int64_t>;

    Range    GetView     () const { return m_view.value_or (GetDefaultView()); }
    void     SetView     (int64_t fromCycle, int64_t toCycle);
    void     ResetView   () { m_view.reset(); }
    bool     IsZoomed    () const { return m_view.has_value(); }

    //  A range of cycles selected, drawn over the bars, or none.
    void     SetSelection (std::optional<Range> selection) { m_selection = selection; }
    Range    GetDataSpan () const;
    int64_t  GetCycleAt  (int x) const;
    float    GetCycleX   (int64_t cycle) const;

    //  The frame's number and cycles, then its routines by their cycles, and
    //  for each line the color its routine is drawn in, otherArgb for the
    //  routines drawn together, 0 for the frame's line.
    static std::wstring  GetFrameTip (const ProfileFrameEntry & frame, const std::vector<ProfileRoutineEntry> & routines,
                                      uint32_t otherArgb, std::vector<uint32_t> & swatches);

    //  The color of a routine's share of a bar.
    static uint32_t      GetShareColor (size_t routine, uint32_t otherArgb);

    bool  TryGetTipAt (POINT point, std::wstring & text, std::vector<uint32_t> & swatches) const;

    //  The pointer's x while it is over the timeline, for the line drawn
    //  there, or none; true when the line moves, appears or goes.
    bool  SetPointer  (std::optional<int> x);

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Profile timeline"; }

    static constexpr size_t  kTipRoutines = 6;

private:
    Range  GetDefaultView () const;

    std::vector<ProfileFrameEntry>    m_frames;
    std::vector<ProfileRoutineEntry>  m_routines;
    DxuiDpiScaler                     m_scaler;
    uint32_t                          m_otherArgb = 0xFF808080;   // the theme's divider color, as last painted
    std::optional<int>                m_pointerX;
    std::optional<Range>              m_view;
    std::optional<Range>              m_selection;
};
