#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendStatus.h"

class IDxuiTheme;
class DxuiTooltip;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel
//
//  The Full Send window's content, top to bottom: what the session is doing
//  or waits for, a panel for each drive with its disk's title, a map of its
//  tracks, a progress bar and its times, the map's legend, the session's
//  totals, the disks done, newest first, and the latest activity lines.
//  In a raw capture a drive's map has a second row, a cell for each
//  position between two tracks that was read.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendPanel : public DxuiPanel
{
public:
    struct View
    {
        std::wstring      connection;
        std::wstring      machine;      // the Apple's model, as Apple //c (ROM 3); empty when it isn't known
        FullSendSnapshot  snapshot;
    };

    static constexpr float  kHeadingDip     = 26.0f;
    static constexpr float  kLineDip        = 20.0f;
    static constexpr float  kSectionGapDip  = 12.0f;
    static constexpr float  kColumnGapDip   = 20.0f;
    static constexpr float  kMapCellDip     = 14.0f;
    static constexpr float  kMapRawDip      = 8.0f;
    static constexpr float  kBarDip         = 10.0f;
    static constexpr float  kSwatchDip      = 10.0f;
    static constexpr float  kFontDip        = 12.0f;
    static constexpr float  kHeadingFontDip = 14.0f;
    static constexpr int    kDiskRows       = 6;
    static constexpr int    kActivityRows   = 6;

    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    //  The marks' colors, which don't change with the theme.
    static uint32_t      GetColor (FullSendTrackMark mark, const IDxuiTheme & theme);
    static uint32_t      GetColor (FullSendPositionMark mark, const IDxuiTheme & theme);

    //  What the session is doing or what it waits for, as a sentence. The
    //  machine is the Apple's model, without its ROM in parentheses; with
    //  none, the sentence has the Apple.
    static std::wstring  DescribeActivity (const FullSendSnapshot & snapshot, const std::wstring & machine = std::wstring());

    //  A drive's state, as Sending, with verification; its times, as 0:21 elapsed,
    //  about 0:14 left; and where its raw capture is.
    static std::wstring  DescribeDrive    (const FullSendDriveStatus & drive);
    static std::wstring  DescribeTimes    (const FullSendDriveStatus & drive);
    static std::wstring  DescribeCapture  (const FullSendDriveStatus & drive);

    static std::wstring  DescribeTotals   (const FullSendTotals & totals);
    static std::wstring  DescribeStatus   (const FullSendRow & row);

    //  The status bar's line for a session in progress: the drive, its disk,
    //  the percentage, the time left and the disks done. Empty with none.
    static std::wstring  DescribeSummary  (const FullSendSnapshot & snapshot);

    //  A map cell's tip: its track, as 12 or 12.25, and what its mark means.
    //  The position is in quarter tracks.
    static std::wstring  DescribeCell     (const FullSendDriveStatus & drive, int position);

    //  Seconds as 1:05, or 1:02:05 from an hour.
    static std::wstring  FormatSeconds    (double seconds);

    void  SetView    (View view);

    //  The tip a map cell under the pointer has.
    void  SetTooltip (DxuiTooltip * tooltip) { m_tooltip = tooltip; }

    bool  OnMouse (const DxuiMouseEvent & ev) override;

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    float  PaintDrive    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int number, const FullSendDriveStatus & drive, float left, float top, float width) const;
    float  PaintLegend   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    float  PaintDisks    (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    float  PaintActivity (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    void   DrawLine      (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, bool heading = false, DxuiTextHAlign align = DxuiTextHAlign::Left) const;

    //  A map cell as it was last painted: its drive's index and its position
    //  in quarter tracks.
    struct Cell
    {
        RECT  bounds   = {};
        int   drive    = 0;
        int   position = 0;
    };

    void   AddCell       (float left, float top, float width, float height, int number, int position) const;

    View                       m_view;
    DxuiDpiScaler              m_scaler;
    DxuiTooltip              * m_tooltip = nullptr;
    mutable std::vector<Cell>  m_cells;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog
//
//  The Full Send window: a connection's session, drawn from its status on
//  each tick. End session cancels the session after a confirmation, Open
//  folder opens the Full Send folder, and Close hides the window while the
//  session goes on.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendDialog : public DxuiDialogWindow
{
public:
    static constexpr int   kWidthDip  = 720;
    static constexpr int   kHeightDip = 650;
    static constexpr int   kEndId     = 1001;
    static constexpr int   kFolderId  = 1002;
    static constexpr UINT  kTickMs    = 250;
    static constexpr UINT  kTipTickMs = 50;     // the tooltip's timer; the view is drawn again every kTickMs

    using SnapshotFn = std::function<bool (FullSendPanel::View & outView)>;
    using ActionFn   = std::function<void()>;

    //  The snapshot function returns false when no connection has a session
    //  to draw, and the view it was given is drawn as it is.
    void  Configure (const IDxuiTheme * theme, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder);

    //  Whether a session is in progress, so End session applies.
    static bool  IsRunning (const FullSendSnapshot & snapshot);

    static std::unique_ptr<FullSendDialog>  Open (HWND owner, const IDxuiTheme * theme, bool activate, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Refresh ();

    const IDxuiTheme  * m_theme     = nullptr;
    SnapshotFn          m_snapshot;
    ActionFn            m_endSession;
    ActionFn            m_openFolder;
    FullSendPanel::View m_view;
    FullSendPanel     * m_panel     = nullptr;
    DxuiButton        * m_endButton = nullptr;
    DxuiTooltip         m_tooltip;
    UINT                m_ticks     = 0;
};
