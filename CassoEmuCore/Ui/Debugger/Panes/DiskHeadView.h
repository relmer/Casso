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
//  stepper phase magnets and the motor, each lit while on.
//
//  The marker sweeps toward the head's track at the drive's stepping speed
//  rather than jumping a frame at a time. It arrives bright and settles to
//  the accent, and each quarter track it leaves fades out behind it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskHeadView : public IDxuiControl
{
public:
    static constexpr int      kRulerDip     = 18;
    static constexpr int      kGapDip       = 4;
    static constexpr int      kRowDip       = 14;
    static constexpr int      kLampDip      = 12;
    static constexpr int      kLampStepDip  = 34;
    static constexpr int      kCaptionDip   = 56;
    static constexpr int      kQuarters     = 4;
    static constexpr int      kMajorTrack   = 5;

    //  DOS 3.3 steps a half track in about 3 ms once it is up to speed.
    static constexpr double   kMsPerQuarter = 1.5;
    static constexpr int64_t  kSettleMs     = 500;      // arrival flash down to the accent
    static constexpr int64_t  kFadeMs       = 1500;     // a track left behind, to gone

    void                         SetHead (const DiagnosticsDiskHead & head);
    const DiagnosticsDiskHead &  GetHead () const                           { return m_head; }

    //  A row more where the drive and track do not fit beside the lamps.
    int                          GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    //  The head marker's left edge and width, in pixels.
    float                        GetHeadX     () const;
    float                        GetHeadWidth () const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                Tick              (int64_t nowMs) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Disk head"; }

private:
    void  PaintRuler (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    void  PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    //  The marker's color: bright on the move and as it arrives, then the
    //  accent; muted while the motor rests.
    uint32_t  GetHeadColor (const IDxuiTheme & theme) const;

    int   GetQuarterCount () const { return std::max (1, m_head.maxQuarterTrack + 1); }

    //  "Drive 1  track 17.25", and whether it goes on a row of its own below
    //  the lamps at this width.
    std::wstring  GetLabel      () const;
    bool          IsLabelBelow  (float widthPx, const DxuiDpiScaler & scaler) const;

    //  1 where the lamp row fits the width, less where it has to shrink.
    float         GetLampScale  (float widthPx) const;

    DiagnosticsDiskHead  m_head;
    DxuiDpiScaler        m_scaler;
    float                m_fontDip = 12.0f;     // the label's font, as last painted

    bool                         m_placed    = false;               // the marker has a position yet
    double                       m_shown     = 0.0;                 // the marker's quarter track, on its way to the head's
    int64_t                      m_nowMs     = 0;
    int64_t                      m_arriveMs  = INT64_MIN / 2;       // when the marker last reached the head
    std::map<int, int64_t>       m_trail;                           // quarter track -> when the marker left it
};
