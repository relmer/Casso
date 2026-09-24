#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView
//
//  The head over the disk's tracks, at quarter-track resolution: a ruler with
//  a tick for every track, a marker where the head is, and below it the four
//  phase magnets and the motor, each lit while on.
//
////////////////////////////////////////////////////////////////////////////////

class DiskHeadView : public IDxuiControl
{
public:
    static constexpr int  kRulerDip     = 18;
    static constexpr int  kGapDip       = 4;
    static constexpr int  kRowDip       = 14;
    static constexpr int  kLampDip      = 12;
    static constexpr int  kLampStepDip  = 34;
    static constexpr int  kQuarters     = 4;
    static constexpr int  kMajorTrack   = 5;

    void                         SetHead (const DiagnosticsDiskHead & head) { m_head = head; }
    const DiagnosticsDiskHead &  GetHead () const                           { return m_head; }

    //  A row more where the drive and track do not fit beside the lamps.
    int                          GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    //  The head marker's left edge and width, in pixels.
    float                        GetHeadX     () const;
    float                        GetHeadWidth () const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Disk head"; }

private:
    void  PaintRuler (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    void  PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    int   GetQuarterCount () const { return std::max (1, m_head.maxQuarterTrack + 1); }

    //  "Drive 1  track 17.25", and whether it goes on a row of its own below
    //  the lamps at this width.
    std::wstring  GetLabel      () const;
    bool          IsLabelBelow  (float widthPx, const DxuiDpiScaler & scaler) const;

    DiagnosticsDiskHead  m_head;
    DxuiDpiScaler        m_scaler;
    float                m_fontDip = 12.0f;     // the label's font, as last painted
};
