#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeterBar
//
//  Named levels from 0 to 1, one row each: the name, then a bar filled in
//  proportion to the level. A level outside the range is drawn at its end.
//
////////////////////////////////////////////////////////////////////////////////

class MeterBar : public IDxuiControl
{
public:
    static constexpr int  kRowDip   = 12;
    static constexpr int  kGapDip   = 2;
    static constexpr int  kLabelDip = 84;

    void                       SetMeters (const DiagnosticsMeters & meters) { m_meters = meters; }
    const DiagnosticsMeters &  GetMeters () const                           { return m_meters; }

    int                        GetPreferredHeightPx (const DxuiDpiScaler & scaler) const;

    //  The filled width of a level, in pixels.
    float                      GetFillWidth (float level) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Meters"; }

private:
    DiagnosticsMeters  m_meters;
    DxuiDpiScaler      m_scaler;
};
