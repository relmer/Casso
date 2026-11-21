#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendStatus.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "Seams/ByteStreamSettings.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView
//
//  One connection's latest disk job, as its tab in the job window draws it:
//  what it does or did, a map of its tracks, its progress and times, and once
//  it ends, what it found.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkJobView
{
    enum class State { Queued, Running, Done, Failed, Canceled };

    static constexpr int  kTracks = 35;

    std::string                                         link;           // the connection, which keys its tab
    std::wstring                                        tabLabel;
    std::wstring                                        connection;     // the Apple and how it's connected, as Apple ][ plus over serial on COM3
    std::wstring                                        heading;
    std::wstring                                        step;           // what a running job is doing now
    std::wstring                                        imagePath;
    State                                               state      = State::Queued;
    int                                                 percent    = 0;
    double                                              elapsed    = 0.0;
    double                                              left       = -1.0;   // seconds, -1 before there is a pace
    bool                                                hasMap     = false;
    std::array<FullSendTrackMark, kTracks>              tracks     = {};
    std::vector<int>                                    weakTracks;
    std::vector<std::pair<std::wstring, std::wstring>>  summary;
    std::wstring                                        message;        // why it failed
    bool                                                isUnseen   = false;  // ended since its tab was last selected

    bool  IsRunning () const { return state == State::Queued || state == State::Running; }

    //  The view of an operation on a connection. The model is the Apple's, as
    //  Apple //c, and empty when it isn't known; elapsed is the seconds since
    //  the operation started.
    static CassoLinkJobView  Build (const LinkOperation & operation, const std::string & link, const std::wstring & model, double elapsed);

    //  The Apple and how it's connected: its model, the transport, and the
    //  COM port for serial.
    static std::wstring  DescribeConnection (const std::wstring & model, LinkTransport transport, const std::wstring & port);

    //  Whether an operation has a job window: a whole disk read, captured or
    //  written.
    static bool  IsDiskJob (LinkOperationKind kind);

    //  A drive, as Slot 6, Drive 2 for a Disk II's.
    static std::wstring  DescribeDrive (int drive);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel
//
//  The job window's content: a tab for each connection with a job, and under
//  it, the selected one's heading and step, its track map, its progress bar
//  and times, and its summary.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkJobPanel : public DxuiPanel
{
public:
    using SelectFn = std::function<void (const std::string & link)>;
    using CloseFn  = std::function<void (const std::string & link)>;

    static constexpr float     kTabsDip        = 34.0f;
    static constexpr float     kPadDip         = 16.0f;
    static constexpr float     kHeadingFontDip = 16.0f;
    static constexpr float     kFontDip        = 12.0f;
    static constexpr float     kLineDip        = 22.0f;
    static constexpr float     kMapCellDip     = 18.0f;
    static constexpr float     kMapGapDip      = 2.0f;
    static constexpr float     kBarDip         = 4.0f;
    static constexpr float     kLabelWidthDip  = 220.0f;
    static constexpr float     kSectionGapDip  = 14.0f;
    static constexpr uint32_t  kWeakArgb       = 0xFFE3B341;
    static constexpr float     kTabWidthDip    = 240.0f;

    //  A map cell's largest size, under the pointer, and the range in tracks
    //  either side over which cells grow, as on the Full Send window's map.
    static constexpr float     kMagnifyMax         = 3.0f;
    static constexpr float     kMagnifyReachTracks = 3.5f;

    //  Whether the map is growing or shrinking, so the window is drawn each
    //  frame until it settles.
    bool  IsMagnifying (int64_t nowMs) const { return m_magnifier.IsEasing (nowMs); }

    void  Init        ();
    void  SetViews    (std::vector<CassoLinkJobView> views, const std::string & selected);
    void  SetOnSelect (SelectFn select) { m_onSelect = std::move (select); }
    void  SetOnClose  (CloseFn close)   { m_onClose = std::move (close); }

    const CassoLinkJobView *  GetSelected () const;

    bool  OnMouse (const DxuiMouseEvent & ev) override;
    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    float  PaintMap     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const CassoLinkJobView & view, float left, float top, float width) const;
    float  PaintSummary (IDxuiTextRenderer & text, const IDxuiTheme & theme, const CassoLinkJobView & view, float left, float top, float width) const;
    void   DrawLine     (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, float fontDip, DxuiFontWeight weight) const;

    void   LayTabs      ();

    DxuiTabStrip                   m_tabs;
    RECT                           m_tabsPx     = {};
    DxuiMagnifiedRow               m_magnifier;
    float                          m_pointerX   = 0.0f;
    mutable RECT                   m_mapPx      = {};   // the map at rest, as last painted
    DxuiDpiScaler                  m_scaler;
    std::vector<CassoLinkJobView>  m_views;
    std::string                    m_selected;
    SelectFn                       m_onSelect;
    CloseFn                        m_onClose;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog
//
//  The job window: CassoLink's disk jobs, a tab for each connection, drawn
//  again on each tick. Cancel stops the selected tab's job; once it ends,
//  Show in folder and Open act on its image. Closing a tab clears it, and
//  the window closes with its last tab.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkJobDialog : public DxuiDialogWindow
{
public:
    static constexpr int   kWidthDip  = 640;
    static constexpr int   kHeightDip = 520;
    static constexpr int   kCancelId  = 1001;
    static constexpr int   kFolderId  = 1002;
    static constexpr int   kOpenId    = 1003;
    static constexpr UINT  kTickMs    = 200;
    static constexpr UINT  kFrameMs   = 16;     // while the map grows or shrinks

    using ViewsFn  = std::function<std::vector<CassoLinkJobView> ()>;
    using LinkFn   = std::function<void (const std::string & link)>;
    using ImageFn  = std::function<void (const std::wstring & imagePath)>;

    struct Actions
    {
        ViewsFn  views;
        LinkFn   cancel;
        LinkFn   closeTab;
        LinkFn   select;
        ImageFn  showInFolder;
        ImageFn  open;
    };

    void  Configure (const IDxuiTheme * theme, Actions actions);
    void  Select    (const std::string & link);

    static std::unique_ptr<CassoLinkJobDialog>  Open (HWND owner, const IDxuiTheme * theme, bool activate, Actions actions, const std::string & link);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Refresh ();

    UINT                 m_tickMs       = kTickMs;
    int64_t              m_refreshedMs  = 0;

    const IDxuiTheme   * m_theme        = nullptr;
    Actions              m_actions;
    std::string          m_selected;
    CassoLinkJobPanel  * m_panel        = nullptr;
    DxuiButton         * m_cancelButton = nullptr;
    DxuiButton         * m_folderButton = nullptr;
    DxuiButton         * m_openButton   = nullptr;
};
