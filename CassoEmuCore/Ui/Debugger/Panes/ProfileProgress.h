#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileProgress
//
//  How much of a profile built from history is done: a track the width of
//  the pane, filled in the accent color to the share replayed, with the
//  share as text over it.
//
//  Bounds are pixels, like every Dxui control.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileProgress : public IDxuiControl
{
public:
    static constexpr int  kHeightDip = 20;

    void    SetShare (double share) { m_share = std::clamp (share, 0.0, 1.0); }
    double  GetShare () const       { return m_share; }

    //  The text over the track, "Loading profile... (42%)".
    static std::wstring  GetText (double share);

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return GetText (m_share); }

private:
    DxuiDpiScaler  m_scaler;
    double         m_share = 0.0;
};
