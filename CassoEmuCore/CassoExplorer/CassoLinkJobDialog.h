#pragma once

#include "Pch.h"

#include "CassoExplorer/FullSendDialog.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "Seams/ByteStreamSettings.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView
//
//  One connection's latest disk job, as its tab in the job window draws it:
//  what it does or did, the file, the Apple and how it's connected, and its
//  drive as the Full Send window draws a drive, with the legend and the
//  job's activity. Once it ends, what it found.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkJobView
{
    enum class State { Queued, Running, Canceling, Done, Failed, Canceled };

    std::string                                         link;           // the connection, which keys its tab
    std::wstring                                        tabLabel;       // as Apple //c (COM3)
    std::wstring                                        connection;     // as Apple //c over serial on COM3
    std::wstring                                        heading;
    std::wstring                                        imagePath;
    State                                               state     = State::Queued;
    std::vector<std::pair<std::wstring, std::wstring>>  summary;
    std::wstring                                        message;        // why it failed
    bool                                                isUnseen  = false;   // ended since its tab was last selected
    bool                                                hasOffer  = false;   // a raw capture or a read again is offered
    FullSendPanel::View                                 drive;

    bool  IsRunning () const { return state == State::Queued || state == State::Running || state == State::Canceling; }

    //  The view of an operation on a connection. The model is the Apple's, as
    //  Apple //c, and empty when it isn't known; the port is the COM port a
    //  serial connection opened.
    static CassoLinkJobView  Build (const LinkOperation & operation, const std::string & link, const std::wstring & model, LinkTransport transport, const std::wstring & port);

    //  Whether an operation has a job window: a whole disk read, captured or
    //  written.
    static bool  IsDiskJob (LinkOperationKind kind);

    //  A drive, as Slot 6, Drive 2 for a Disk II's.
    static std::wstring  DescribeDrive (int drive);

    //  The Apple and how it's connected, as Apple //c over serial on COM3;
    //  and shorter, for a tab, as Apple //c (COM3).
    static std::wstring  DescribeConnection (const std::wstring & model, LinkTransport transport, const std::wstring & port);
    static std::wstring  DescribeTab        (const std::wstring & model, const std::string & link, LinkTransport transport, const std::wstring & port);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel
//
//  The job window's content: a tab for each connection with a job, and under
//  it, the selected one's status line, a link to its file, the Apple and its
//  connection, what it found once it ends, and its drive in the Full Send
//  window's panel.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkJobPanel : public DxuiPanel
{
public:
    using LinkFn  = std::function<void (const std::string & link)>;
    using ImageFn = std::function<void (const std::wstring & imagePath)>;

    static constexpr float  kTabsDip       = 34.0f;
    static constexpr float  kTabWidthDip   = 180.0f;
    static constexpr float  kPadDip        = 16.0f;
    static constexpr float  kStatusFontDip = 14.0f;
    static constexpr float  kFontDip       = 12.0f;
    static constexpr float  kLineDip       = 22.0f;
    static constexpr float  kLabelWidthDip = 220.0f;
    static constexpr float  kSectionGapDip = 12.0f;
    static constexpr float  kHeadingShare  = 0.6f;    // the status line's share of the first row; the connection is right-aligned in the rest
    static constexpr float  kPageShade     = 0.07f;   // the page and its tab, mixed this far toward the text color
    static constexpr float  kFooterDip     = 32.0f;   // the row under the page, for the checkbox

    void  Init          (DxuiTooltip * tooltip);
    void  SetViews      (std::vector<CassoLinkJobView> views, const std::string & selected);
    void  SetOnSelect   (LinkFn select)  { m_onSelect = std::move (select); }
    void  SetOnClose    (LinkFn close)   { m_onClose = std::move (close); }
    void  SetOnOpenFile (ImageFn open)   { m_onOpenFile = std::move (open); }

    const CassoLinkJobView  * GetSelected  () const;
    DxuiCheckbox            & GetAutoOpen  () { return m_autoOpen; }
    bool                      IsMagnifying (int64_t nowMs) const { return m_drive.IsMagnifying (nowMs); }

    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    void   LayTabs   ();
    void   LayDrive  ();
    float  GetHeader (const CassoLinkJobView * view) const;

    static bool  HasResult (const CassoLinkJobView & view);
    void         PaintLink (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & path, float left, float top, float width) const;
    void   DrawLine  (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, float fontDip,
                      DxuiTextHAlign align = DxuiTextHAlign::Left, DxuiFontWeight weight = DxuiFontWeight::Normal) const;

    DxuiTabStrip                   m_tabs;
    FullSendPanel                  m_drive;
    DxuiCheckbox                   m_autoOpen   { L"Open this dialog automatically when a transfer begins", true };
    RECT                           m_tabsPx     = {};
    DxuiDpiScaler                  m_scaler;
    std::vector<CassoLinkJobView>  m_views;
    std::string                    m_selected;
    mutable RECT                   m_linkPx     = {};   // the image's link, as last painted
    bool                           m_isOverLink = false;
    LinkFn                         m_onSelect;
    LinkFn                         m_onClose;
    ImageFn                        m_onOpenFile;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog
//
//  The job window: CassoLink's disk jobs, a tab for each connection, drawn
//  again on each tick. Cancel job stops the selected tab's job; a read that
//  couldn't read every sector offers a raw capture or a read again; once it
//  ends, Open opens its image. Closing a tab clears it, and the window closes
//  with its last tab.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkJobDialog : public DxuiDialogWindow
{
public:
    static constexpr int   kWidthDip   = 640;
    static constexpr int   kHeightDip  = 720;
    static constexpr int   kCancelId   = 1001;
    static constexpr int   kOpenId     = 1003;
    static constexpr int   kRawId      = 1004;
    static constexpr int   kAgainId    = 1005;
    static constexpr UINT  kTickMs     = 200;
    static constexpr UINT  kTipTickMs  = 50;     // the tooltip's timer
    static constexpr UINT  kFrameMs    = 16;     // while the map grows or shrinks

    using ViewsFn  = std::function<std::vector<CassoLinkJobView> ()>;
    using LinkFn   = std::function<void (const std::string & link)>;
    using ImageFn  = std::function<void (const std::wstring & imagePath)>;

    struct Actions
    {
        ViewsFn  views;
        LinkFn   cancel;
        LinkFn   closeTab;
        LinkFn   select;
        LinkFn   captureRaw;
        LinkFn   readAgain;
        ImageFn  showInFolder;
        ImageFn  open;

        //  The checkbox under the page: its state when the window opens, and
        //  each change.
        bool                        autoOpen = true;
        std::function<void (bool)>  setAutoOpen;
    };

    void  Configure (const IDxuiTheme * theme, Actions actions);
    void  Select    (const std::string & link);

    static std::unique_ptr<CassoLinkJobDialog>  Open (HWND owner, const IDxuiTheme * theme, bool activate, Actions actions, const std::string & link);

protected:
    void  OnCreate      () override;
    void  OnDialogTick  () override;
    void  OnWindowClose () override;

private:
    void  Refresh       ();
    void  RunOnSelected (const LinkFn & action);

    const IDxuiTheme   * m_theme        = nullptr;
    Actions              m_actions;
    std::string          m_selected;
    CassoLinkJobPanel  * m_panel        = nullptr;
    DxuiButton         * m_cancelButton = nullptr;
    DxuiButton         * m_rawButton    = nullptr;
    DxuiButton         * m_againButton  = nullptr;
    DxuiButton         * m_openButton   = nullptr;
    DxuiTooltip          m_tooltip;
    UINT                 m_tickMs       = kTipTickMs;
    int64_t              m_refreshedMs  = 0;
};
