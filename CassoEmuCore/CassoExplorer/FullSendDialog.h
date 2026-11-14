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
//  position between two tracks that was read. Each other connection with a
//  session in progress has a section after the drives: its state, its
//  drives and its totals.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendPanel : public DxuiPanel
{
public:
    struct Section
    {
        std::wstring      connection;
        std::wstring      machine;      // the Apple's model, as Apple //c (ROM 3); empty when it isn't known
        FullSendSnapshot  snapshot;
    };

    struct View : Section
    {
        std::vector<Section>  others;   // each other connection with a session in progress
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
    static constexpr float  kOtherDip       = 215.0f;   // an other connection's section

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

    //  The activity lines as text, each after its time, one to a line.
    static std::wstring  FormatActivity   (const FullSendSnapshot & snapshot);

    //  A folder's manifest as the disks done, newest first, for the window
    //  with no session. False for bytes that aren't a manifest.
    static bool          MakeManifestView (std::span<const Byte> manifest, FullSendSnapshot & outSnapshot);

    void  SetView    (View view);

    //  The tip a map cell under the pointer has.
    void  SetTooltip (DxuiTooltip * tooltip) { m_tooltip = tooltip; }

    //  A double-click on a disk's row, by its file; and a right-click on a
    //  disk's row or on the activity list, by the row's file or an empty one
    //  for the list, at the point clicked.
    using DiskFn = std::function<void (const std::string & file)>;
    using MenuFn = std::function<void (const std::string & file, POINT pointPx)>;

    void  SetOnOpenDisk (DiskFn open) { m_onOpen = std::move (open); }
    void  SetOnMenu     (MenuFn menu) { m_onMenu = std::move (menu); }

    //  The file of the disk's row under a point, as last painted; false for
    //  none, or for a duplicate's row, which has no image.
    bool  TryGetDiskAt   (POINT pointPx, std::string & outFile) const;
    bool  IsOverActivity (POINT pointPx) const;

    bool  OnMouse (const DxuiMouseEvent & ev) override;

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    float  PaintDrives   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int section, const FullSendSnapshot & snapshot, float top) const;
    float  PaintDrive    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int section, int number, const FullSendDriveStatus & drive, float left, float top, float width) const;
    float  PaintLegend   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    float  PaintDisks    (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    float  PaintActivity (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const;
    void   DrawLine      (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, bool heading = false, DxuiTextHAlign align = DxuiTextHAlign::Left) const;

    //  A map cell as it was last painted: its section, 0 for the view's
    //  connection and 1 on for the others, its drive's index and its position
    //  in quarter tracks.
    struct Cell
    {
        RECT  bounds   = {};
        int   section  = 0;
        int   drive    = 0;
        int   position = 0;
    };

    void   AddCell       (float left, float top, float width, float height, int section, int number, int position) const;
    bool   HandleClick   (const DxuiMouseEvent & ev);

    //  A disk's row as it was last painted.
    struct Row
    {
        RECT         bounds = {};
        std::string  file;
    };

    View                       m_view;
    DxuiDpiScaler              m_scaler;
    DxuiTooltip              * m_tooltip     = nullptr;
    mutable std::vector<Cell>  m_cells;
    mutable std::vector<Row>   m_rows;
    mutable RECT               m_activity    = {};
    DiskFn                     m_onOpen;
    MenuFn                     m_onMenu;
    std::string                m_lastClicked;
    int64_t                    m_lastClickMs = 0;
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

    //  The actions on a disk's row: a double-click selects its image in the
    //  folder, and its menu has Open, Inspect disk image and Disk properties.
    enum class DiskAction { Select, Open, Inspect, Properties };

    using DiskActionFn = std::function<void (const std::string & file, DiskAction action)>;
    using MovedFn      = std::function<void (POINT topLeftPx)>;

    //  The snapshot function returns false when no connection has a session
    //  to draw, and the view it was given is drawn as it is.
    void  Configure (const IDxuiTheme * theme, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder);

    //  Whether a session is in progress, so End session applies.
    static bool  IsRunning (const FullSendSnapshot & snapshot);

    //  At position, the window's top left on the screen, when it is on a
    //  monitor; centered on the owner otherwise.
    static std::unique_ptr<FullSendDialog>  Open (HWND owner, const IDxuiTheme * theme, bool activate, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder,
                                                  const POINT * position = nullptr);

    void  SetDiskAction (DiskActionFn action) { m_diskAction = std::move (action); }
    void  SetOnMoved    (MovedFn moved)       { m_onMoved = std::move (moved); }

    //  The menu of a disk's row, or of the activity list for an empty file.
    std::vector<DxuiPopupMenuItem>  MakeMenu (const std::string & file);

    FullSendPanel  * GetPanel () const { return m_panel; }

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Refresh      ();
    void  NoteMoved    ();
    void  AddCommand   (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch);

    const IDxuiTheme                           * m_theme        = nullptr;
    SnapshotFn                                   m_snapshot;
    ActionFn                                     m_endSession;
    ActionFn                                     m_openFolder;
    FullSendPanel::View                          m_view;
    FullSendPanel                              * m_panel        = nullptr;
    DxuiButton                                 * m_endButton    = nullptr;
    DxuiTooltip                                  m_tooltip;
    UINT                                         m_ticks        = 0;
    DiskActionFn                                 m_diskAction;
    MovedFn                                      m_onMoved;
    POINT                                        m_lastTopLeft  = { LONG_MIN, LONG_MIN };
    size_t                                       m_othersRoom   = 0;    // the other connections' sections the window has grown for
    std::vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;   // the open menu's commands
};
