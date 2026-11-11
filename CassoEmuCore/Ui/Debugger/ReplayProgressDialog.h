#pragma once

#include "Debugger/Reverse/ReplayControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressPanel
//
//  The search's text above a bar filled to how much of history it has
//  covered.
//
////////////////////////////////////////////////////////////////////////////////

class ReplayProgressPanel : public DxuiPanel
{
public:
    static constexpr int  kTextDip = 24;
    static constexpr int  kGapDip  = 10;
    static constexpr int  kBarDip  = 16;

    void  SetProgress (const std::wstring & text, float fraction) { m_text = text; m_fraction = fraction; }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    DxuiDpiScaler  m_scaler;
    std::wstring   m_text;
    float          m_fraction = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog
//
//  A reverse continue's search, while it runs past kShowAfterMs: the share of
//  history it has covered as a bar and a percentage, and Stop, which ends the
//  search where it is. The debugger window is disabled while it is open,
//  and it closes when the search ends.
//
////////////////////////////////////////////////////////////////////////////////

class ReplayProgressDialog : public DxuiDialogWindow
{
public:
    static constexpr uint64_t  kShowAfterMs = 400;
    static constexpr int       kWidthDip    = 420;
    static constexpr int       kHeightDip   = 170;
    static constexpr UINT      kTickMs      = 100;

    using ProgressFn = std::function<ReplayProgress ()>;
    using StopFn     = std::function<void ()>;

    //  Whether the dialog is due for a replay's progress: one that has run
    //  past kShowAfterMs and reports its progress, since only that kind can
    //  be stopped.
    static bool          IsDue      (const ReplayProgress & progress);

    //  The text over the bar: the percentage covered, or Stopping once Stop
    //  is pressed.
    static std::wstring  GetText    (const ReplayProgress & progress, bool isStopping);

    static std::unique_ptr<ReplayProgressDialog>  Open (HWND owner, const IDxuiTheme * theme, ProgressFn progress, StopFn stop);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Refresh ();

    ProgressFn             m_progress;
    StopFn                 m_stop;
    bool                   m_isStopping = false;
    ReplayProgressPanel  * m_panel      = nullptr;
    DxuiButton           * m_stopButton = nullptr;
};
