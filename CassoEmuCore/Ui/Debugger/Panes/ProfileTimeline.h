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

    //  The frame's number and cycles, then its routines by their cycles.
    static std::wstring  GetFrameTip (const ProfileFrameEntry & frame, const std::vector<ProfileRoutineEntry> & routines);

    bool  TryGetTipAt (POINT point, std::wstring & text) const;

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
};
