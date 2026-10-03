#pragma once

#include "Pch.h"
#include "Debugger/Reverse/HistoryStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBand
//
//  The band across the top of the disassembly and registers panes while the
//  machine stands in its recorded history: how far behind live it is, in
//  instructions and in time, how the last reverse command ended when it did
//  not simply move, and a Go live link that returns the machine to where it
//  was running. It is drawn in the theme's information banner colors, or its
//  warning colors for an outcome that stopped short.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryBand : public IDxuiControl
{
public:
    static constexpr int  kHeightDip = 24;

    HistoryBand() = default;
    ~HistoryBand() override = default;

    void  SetStatus  (const HistoryStatus & status) { m_status = status; }
    void  SetOnGoLive (std::function<void()> onGoLive) { m_onGoLive = std::move (onGoLive); }

    const HistoryStatus &  GetStatus () const { return m_status; }

    //  Whether the band shows: behind live, or after a reverse command that
    //  stopped short of where it was going.
    static bool          IsShown       (const HistoryStatus & status);

    //  Whether the Go live link shows, which is while the machine is behind
    //  live.
    static bool          CanGoLive     (const HistoryStatus & status) { return status.isBehindLive; }

    //  The band's text: the outcome first, then the distance behind live.
    static std::wstring  GetText       (const HistoryStatus & status);
    static std::wstring  GetOutcomeText (ReverseOutcome outcome);
    static std::wstring  GetDistanceText (uint64_t instructions, uint64_t cycles);
    static std::wstring  GroupDigits   (uint64_t value);

    static constexpr const wchar_t * kGoLiveText = L"Go live";

    void          Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void          Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool          OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR       GetCursorForPoint (POINT clientPx) const override;

    std::wstring        GetAccessibleName () const override { return GetText (m_status); }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    static constexpr double  kCyclesPerSecond = 1020484.0;
    static constexpr float   kPadDip          = 8.0f;
    static constexpr float   kEdgeDip         = 1.0f;

    bool          IsOverLink        (POINT point) const;

    HistoryStatus           m_status;
    std::function<void()>   m_onGoLive;
    DxuiDpiScaler           m_scaler;
    RECT                    m_linkRect = {};
};
