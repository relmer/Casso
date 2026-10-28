#pragma once

#include "Pch.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileTimeline
//
//  The profile's last frames across the width, newest at the right, each a
//  bar as tall as its share of a frame's cycles, stacked by routine in the
//  flame graph's colors for the kColoredRoutines hottest routines, the rest
//  of its cycles together in the divider color. A frame's tip lists its
//  routines.
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

    //  The frame's number and cycles, then its routines by their cycles, and
    //  for each line the color its routine is drawn in, otherArgb for the
    //  routines drawn together, 0 for the frame's line.
    static std::wstring  GetFrameTip (const ProfileFrameEntry & frame, const std::vector<ProfileRoutineEntry> & routines,
                                      uint32_t otherArgb, std::vector<uint32_t> & swatches);

    //  The color of a routine's share of a bar.
    static uint32_t      GetShareColor (size_t routine, uint32_t otherArgb);

    bool  TryGetTipAt (POINT point, std::wstring & text, std::vector<uint32_t> & swatches) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Profile timeline"; }

    static constexpr size_t  kTipRoutines = 6;

private:
    //  Where a slot starts, the oldest of RoutineProfile::kFrameCount at the
    //  left.
    float  GetSlotX (size_t slot) const;

    std::vector<ProfileFrameEntry>    m_frames;
    std::vector<ProfileRoutineEntry>  m_routines;
    DxuiDpiScaler                     m_scaler;
    uint32_t                          m_otherArgb = 0xFF808080;   // the theme's divider color, as last painted
};
