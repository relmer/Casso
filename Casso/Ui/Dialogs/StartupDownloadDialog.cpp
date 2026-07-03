#include "Pch.h"

#include "StartupDownloadDialog.h"

#include "DialogDefinition.h"
#include "../Chrome/CassoTheme.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiEvents.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"
#include "Window/DxuiDialogWindow.h"
#include "Widgets/DxuiButton.h"
#include "Core/UnicodeSymbols.h"


namespace fs = std::filesystem;


static constexpr int    s_kRowHeightDp        = 28;
static constexpr int    s_kRowGapDp           = 2;
static constexpr int    s_kBodyWidthDp        = 560;
static constexpr int    s_kHeaderHeightDp     = 26;
static constexpr int    s_kHeaderGapAboveDp   = 12;
static constexpr float  s_kFontDp             = 13.0f;
static constexpr float  s_kHeaderFontDp       = 13.0f;
static constexpr float  s_kSourceColumnDp     = 170.0f;
static constexpr float  s_kStatusColumnDp     = 64.0f;
static constexpr float  s_kColumnGapDp        = 12.0f;
static constexpr unsigned int  s_kTickIntervalMs = 100;

static constexpr int    s_kIdDownload = 100;
static constexpr int    s_kIdSkip     = 101;





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDownloadDialog: nested type definitions
//
////////////////////////////////////////////////////////////////////////////////

enum class StartupDownloadDialog::EntryStatus
{
    Pending,
    Downloading,
    Done,
    Failed,
    Cancelled,
    Skipped
};



struct StartupDownloadDialog::EntryRuntime
{
    std::atomic<std::uint64_t>  bytesDone{0};
    std::atomic<int>            status{(int) EntryStatus::Pending};
    std::string                 errorMsg;
    bool                        startedWrite = false;
};



struct StartupDownloadDialog::DialogState
{
    StartupDownloadSet  *        set         = nullptr;
    std::vector<EntryRuntime>    runtime;
    std::vector<DxuiCheckbox>        checkboxes;     // parallel to entries
    std::vector<std::thread>     workers;
    std::atomic<bool>            cancelFlag{false};
    std::atomic<int>             workersInFlight{0};
    std::atomic<bool>            anyFailed  {false};
    bool                         downloading    = false;
    bool                         finished       = false;
    bool                         showStatus     = false;  // gates per-row status text
    UINT                         dpi            = 96;
    int                          bodyOriginXPx  = 0;
    int                          bodyOriginYPx  = 0;
    StartupDownloadResult        result         = StartupDownloadResult::Skipped;
};



struct StartupDownloadDialog::RowMetrics
{
    float  x         = 0.0f;
    float  fullW     = 0.0f;
    float  rowH      = 0.0f;
    float  headerH   = 0.0f;
    float  headerGap = 0.0f;
    float  gap       = 0.0f;
    float  sourceW   = 0.0f;
    float  statusW   = 0.0f;
    float  colGap    = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WorkerThreadProc
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::WorkerThreadProc (DialogState * state, size_t index)
{
    StartupAssetEntry & entry = state->set->entries[index];
    EntryRuntime      & rt    = state->runtime[index];
    HRESULT             hr    = S_OK;

    rt.status.store ((int) EntryStatus::Downloading, std::memory_order_relaxed);
    rt.startedWrite = true;

    if (entry.downloadFn)
    {
        hr = entry.downloadFn (rt.bytesDone, state->cancelFlag, rt.errorMsg);
    }
    else
    {
        rt.errorMsg = "No downloader registered";
        hr = E_NOTIMPL;
    }

    if (hr == E_ABORT || state->cancelFlag.load (std::memory_order_relaxed))
    {
        rt.status.store ((int) EntryStatus::Cancelled, std::memory_order_relaxed);
    }
    else if (FAILED (hr))
    {
        rt.status.store ((int) EntryStatus::Failed, std::memory_order_relaxed);
        state->anyFailed.store (true, std::memory_order_relaxed);
    }
    else
    {
        rt.status.store ((int) EntryStatus::Done, std::memory_order_relaxed);
    }

    state->workersInFlight.fetch_sub (1, std::memory_order_acq_rel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartWorkers
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::StartWorkers (DialogState & state)
{
    for (size_t i = 0; i < state.set->entries.size(); i++)
    {
        if (!state.set->entries[i].selected)
        {
            state.runtime[i].status.store ((int) EntryStatus::Skipped, std::memory_order_relaxed);
            continue;
        }

        state.workersInFlight.fetch_add (1, std::memory_order_acq_rel);
        state.workers.emplace_back (WorkerThreadProc, &state, i);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  JoinAllWorkers
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::JoinAllWorkers (DialogState & state)
{
    for (std::thread & t : state.workers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    state.workers.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovePartialFiles
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::RemovePartialFiles (DialogState & state)
{
    std::error_code  ec;

    for (size_t i = 0; i < state.set->entries.size(); i++)
    {
        EntryStatus  s = (EntryStatus) state.runtime[i].status.load (std::memory_order_relaxed);

        if (s == EntryStatus::Done)
        {
            continue;
        }

        if (!state.runtime[i].startedWrite)
        {
            continue;
        }

        for (const fs::path & p : state.set->entries[i].destPaths)
        {
            if (!p.empty())
            {
                fs::remove (p, ec);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StartupDownloadDialog::StatusText (const EntryRuntime & rt, std::uint64_t expected)
{
    EntryStatus    s    = (EntryStatus) rt.status.load (std::memory_order_relaxed);
    std::uint64_t  done = rt.bytesDone.load (std::memory_order_relaxed);

    switch (s)
    {
        case EntryStatus::Pending:     return L"Waiting";
        case EntryStatus::Skipped:     return L"";
        case EntryStatus::Done:        return L"Done";
        case EntryStatus::Failed:      return L"Failed";
        case EntryStatus::Cancelled:   return L"Cancelled";
        case EntryStatus::Downloading:
            break;
    }

    if (expected == 0)
    {
        // Unknown size: simple busy indicator.
        return L"...";
    }

    int  pct = (int) ((100ull * done) / expected);

    if (pct > 100)
    {
        pct = 100;
    }

    wchar_t buf[16] = {};
    swprintf_s (buf, L"%d%%", pct);
    return buf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintGroupHeader
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::PaintGroupHeader (
    DialogPaintContext   & ctx,
    DxuiLabel                & hdrLabel,
    const std::wstring   & groupLabel,
    const RowMetrics     & m,
    float                  y)
{
    hdrLabel.SetText (groupLabel);
    hdrLabel.SetRect ({ (LONG) m.x, (LONG) y,
                        (LONG) (m.x + m.fullW), (LONG) (y + m.headerH) });
    hdrLabel.Paint   (*ctx.painter, *ctx.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintEntryRow
//
//  Paints one tree-leaf row: DxuiCheckbox + (label inside checkbox) on the
//  left, dim source column, optional right-aligned status.
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::PaintEntryRow (
    DialogPaintContext        & ctx,
    const StartupAssetEntry   & entry,
    DxuiCheckbox                  & cb,
    DxuiLabel                     & sourceLabel,
    DxuiLabel                     & statusLabel,
    const std::wstring        & status,
    bool                        downloading,
    bool                        showStatus,
    const RowMetrics          & m,
    float                       y)
{
    float  cbAvailW = m.fullW - m.sourceW - m.statusW - m.colGap * 2.0f;
    RECT   cbRect   = { (LONG) m.x,
                        (LONG) y,
                        (LONG) (m.x + cbAvailW),
                        (LONG) (y + m.rowH) };



    cb.SetChecked (entry.selected);
    cb.SetEnabled (entry.selectable && !downloading);
    cb.SetRect    (cbRect);
    cb.SetLabel   (entry.displayName);
    cb.Paint      (*ctx.painter, *ctx.text, *ctx.theme);

    sourceLabel.SetText (entry.source);
    sourceLabel.SetRect ({ (LONG) (m.x + cbAvailW + m.colGap), (LONG) y,
                           (LONG) (m.x + cbAvailW + m.colGap + m.sourceW), (LONG) (y + m.rowH) });
    sourceLabel.Paint   (*ctx.painter, *ctx.text);

    if (showStatus && entry.selected)
    {
        statusLabel.SetText (status);
        statusLabel.SetRect ({ (LONG) (m.x + m.fullW - m.statusW), (LONG) y,
                               (LONG) (m.x + m.fullW),             (LONG) (y + m.rowH) });
        statusLabel.Paint   (*ctx.painter, *ctx.text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintBody
//
//  Single-line tree rows. Group headers are bold no-row; each entry row
//  paints a DxuiCheckbox widget on the left, then a dim source label, then
//  (when status is showing) a right-aligned percent / Done / Failed
//  indicator.
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::PaintBody (
    DialogPaintContext & ctx,
    StartupDownloadSet & set,
    DialogState        & state)
{
    RowMetrics  m         = {};
    float       y         = 0.0f;
    uint32_t    fg        = 0;
    uint32_t    fgDim     = 0;
    uint32_t    hdrFg     = 0;
    wstring     curGroup;
    DxuiLabel       hdrLabel;
    DxuiLabel       sourceLabel;
    DxuiLabel       statusLabel;



    if (ctx.painter == nullptr || ctx.text == nullptr || ctx.theme == nullptr)
    {
        return;
    }

    m.x         = (float) ctx.customBodyRect.left;
    m.fullW     = (float) (ctx.customBodyRect.right - ctx.customBodyRect.left);
    m.rowH      = (float) s_kRowHeightDp      * ctx.dpiScale;
    m.headerH   = (float) s_kHeaderHeightDp   * ctx.dpiScale;
    m.headerGap = (float) s_kHeaderGapAboveDp * ctx.dpiScale;
    m.gap       = (float) s_kRowGapDp         * ctx.dpiScale;
    m.sourceW   = s_kSourceColumnDp           * ctx.dpiScale;
    m.statusW   = s_kStatusColumnDp           * ctx.dpiScale;
    m.colGap    = s_kColumnGapDp              * ctx.dpiScale;

    y     = (float) ctx.customBodyRect.top;
    fg    = ctx.theme->dropdownItemText;
    fgDim = (fg & 0x00FFFFFFu) | 0x70000000u;
    hdrFg = ctx.theme->titleText;

    state.bodyOriginXPx = ctx.customBodyRect.left;
    state.bodyOriginYPx = ctx.customBodyRect.top;

    hdrLabel.SetDpi         (state.dpi);
    hdrLabel.SetFontSizeDip (s_kHeaderFontDp);
    hdrLabel.SetColorArgb   (hdrFg);
    hdrLabel.SetFontWeight  (DxuiFontWeight::Bold);

    sourceLabel.SetDpi         (state.dpi);
    sourceLabel.SetFontSizeDip (s_kFontDp);
    sourceLabel.SetColorArgb   (fgDim);

    statusLabel.SetDpi         (state.dpi);
    statusLabel.SetFontSizeDip (s_kFontDp);
    statusLabel.SetColorArgb   (fg);
    statusLabel.SetTextAlign   (DxuiTextRenderer::HAlign::Right, DxuiTextRenderer::VAlign::Center);

    for (size_t i = 0; i < set.entries.size(); i++)
    {
        const StartupAssetEntry & entry  = set.entries[i];
        const EntryRuntime      & rt     = state.runtime[i];
        std::wstring              status = state.showStatus
                                              ? StatusText (rt, entry.expectedBytes)
                                              : wstring();

        if (entry.groupLabel != curGroup)
        {
            // Margin above every group header -- including the first, so
            // the header clears the intro paragraph above the body. The
            // height reservation counts this gap for all headers, so
            // applying it uniformly keeps paint and reserved height in
            // step (no dead space at the bottom).
            y += m.headerGap;

            curGroup = entry.groupLabel;
            PaintGroupHeader (ctx, hdrLabel, curGroup, m, y);
            y += m.headerH + m.gap;
        }

        PaintEntryRow (ctx, entry, state.checkboxes[i], sourceLabel, statusLabel,
                       status, state.downloading, state.showStatus, m, y);
        y += m.rowH + m.gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleBodyInput
//
//  Forwards mouse events to per-row DxuiCheckbox widgets. Coordinates arrive
//  body-relative; the DxuiCheckbox widget hit-tests against absolute window
//  coordinates (the same space its rect is set to during paint), so the
//  cached body origin is added back.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> StartupDownloadDialog::HandleBodyInput (const DialogInputEvent & ev, DialogState & state)
{
    int  absX = 0;
    int  absY = 0;



    if (state.downloading)
    {
        return std::nullopt;
    }

    absX = ev.xPx + state.bodyOriginXPx;
    absY = ev.yPx + state.bodyOriginYPx;

    for (DxuiCheckbox & cb : state.checkboxes)
    {
        switch (ev.kind)
        {
            case DialogInputEvent::Kind::LeftButtonDown:
                cb.OnLButtonDown (absX, absY);
                break;

            case DialogInputEvent::Kind::LeftButtonUp:
                cb.OnLButtonUp (absX, absY);
                break;

            case DialogInputEvent::Kind::MouseMove:
                cb.SetMouseHover (absX, absY);
                break;

            case DialogInputEvent::Kind::KeyDown:
                break;
        }
    }

    return std::nullopt;
}





namespace
{
    //
    //  DownloadBodyPanel -- paint/input bridge that renders the asset rows
    //  via the existing PaintBody callback and forwards mouse events to the
    //  per-row checkboxes via HandleBodyInput. It draws through the concrete
    //  DxuiPainter / DxuiTextRenderer / CassoTheme (the modal host's actual
    //  types) that the legacy DialogPaintContext expects.
    //
    class DownloadBodyPanel : public DxuiPanel
    {
    public:
        void  SetPaintFn (std::function<void (DialogPaintContext &)>     fn) { m_paint = std::move (fn); }
        void  SetInputFn (std::function<void (const DialogInputEvent &)> fn) { m_input = std::move (fn); }

        void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
        {
            SetBounds  (boundsPx);
            m_dpiScale = (float) scaler.Dpi() / (float) DxuiDpiScaler::kBaseDpi;
        }

        void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
        {
            DialogPaintContext  ctx;

            ctx.painter        = static_cast<DxuiPainter *> (&painter);
            ctx.text           = static_cast<DxuiTextRenderer *> (&text);
            ctx.theme          = static_cast<const CassoTheme *> (&theme);
            ctx.customBodyRect = Bounds();
            ctx.dpiScale       = m_dpiScale;

            if (m_paint)
            {
                m_paint (ctx);
            }
        }

        bool  OnMouse (const DxuiMouseEvent & ev) override
        {
            DialogInputEvent  die;
            RECT              b        = Bounds();
            bool              consumed = true;


            die.xPx = ev.positionDip.x - b.left;
            die.yPx = ev.positionDip.y - b.top;

            switch (ev.kind)
            {
            case DxuiMouseEventKind::Down: die.kind = DialogInputEvent::Kind::LeftButtonDown; break;
            case DxuiMouseEventKind::Up:   die.kind = DialogInputEvent::Kind::LeftButtonUp;   break;
            case DxuiMouseEventKind::Move: die.kind = DialogInputEvent::Kind::MouseMove;      break;
            default:                       consumed = false;                                  break;
            }

            if (consumed && m_input)
            {
                m_input (die);
            }

            return consumed;
        }


    private:
        std::function<void (DialogPaintContext &)>      m_paint;
        std::function<void (const DialogInputEvent &)>  m_input;
        float                                           m_dpiScale = 1.0f;
    };



    //
    //  DownloadContentPanel -- stacks the intro label above the asset-row
    //  body, laid out in physical pixels.
    //
    class DownloadContentPanel : public DxuiPanel
    {
    public:
        void  Init (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip)
        {
            m_intro          = intro;
            m_body           = body;
            m_introHeightDip = introHeightDip;

            Adopt (*intro);
            Adopt (*body);
        }

        void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
        {
            int  ih = scaler.Px (m_introHeightDip);


            SetBounds (boundsPx);

            if (m_intro != nullptr)
            {
                RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + ih };

                m_intro->Layout (r, scaler);
            }

            if (m_body != nullptr)
            {
                RECT  r = { boundsPx.left, boundsPx.top + ih, boundsPx.right, boundsPx.bottom };

                m_body->Layout (r, scaler);
            }
        }


    private:
        DxuiLabel          *  m_intro          = nullptr;
        DownloadBodyPanel  *  m_body           = nullptr;
        int                   m_introHeightDip = 0;
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DownloadDialog
    //
    //  DxuiDialogWindow hosting the intro + asset-row content and the
    //  Download / Skip / Exit buttons. Download carries a custom click
    //  handler (start workers, relabel -- it must NOT close); Skip auto-
    //  closes (leaving the default Skipped result); Exit is the IDCANCEL
    //  button (Escape / close-box) with a custom handler that records the
    //  Exit result. A fast tick polls the workers via the OnPoll hook.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DownloadDialog : public DxuiDialogWindow
    {
    public:
        void  ConfigureDownload (std::unique_ptr<DxuiPanel> content, bool requiresRoms, unsigned tickMs)
        {
            m_pendingContent = std::move (content);
            m_requiresRoms   = requiresRoms;
            m_tickMs         = tickMs;
        }

        void  SetOnDownloadClick (std::function<void()> fn) { m_onDownloadClick = std::move (fn); }
        void  SetOnPoll          (std::function<void()> fn) { m_onPoll          = std::move (fn); }

        DxuiButton *  DownloadButton () const { return m_downloadBtn; }
        DxuiButton *  SkipButton     () const { return m_skipBtn; }
        DxuiButton *  ExitButton     () const { return m_exitBtn; }


    protected:
        void  OnCreate () override
        {
            if (m_pendingContent != nullptr)
            {
                SetDialogContentOwned (std::move (m_pendingContent));
            }

            m_downloadBtn = AddDialogButton (L"Download", s_kIdDownload);
            m_downloadBtn->SetOnClick ([this] () { if (m_onDownloadClick) { m_onDownloadClick(); } });

            if (!m_requiresRoms)
            {
                m_skipBtn = AddDialogButton (L"Skip", s_kIdSkip);
            }

            m_exitBtn = AddDialogButton (L"Exit", IDCANCEL);

            SetModalTickIntervalMs (m_tickMs);
        }

        void  OnDialogTick () override
        {
            if (m_onPoll)
            {
                m_onPoll();
            }
        }


    private:
        std::unique_ptr<DxuiPanel>  m_pendingContent;
        bool                        m_requiresRoms    = false;
        unsigned                    m_tickMs          = 100;
        std::function<void()>       m_onDownloadClick;
        std::function<void()>       m_onPoll;
        DxuiButton  *               m_downloadBtn     = nullptr;
        DxuiButton  *               m_skipBtn         = nullptr;
        DxuiButton  *               m_exitBtn         = nullptr;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDownloadDialog::Show
//
//  Constructs and modally displays the unified startup-asset download
//  dialog through the Dxui host: an intro label above a per-asset checkbox
//  list with live progress, plus Download / Skip / Exit buttons. Download
//  starts the workers in place (the button stays open and relabels); a tick
//  polls the workers and closes when done. On exit, cancels + joins any
//  still-running workers, scrubs partial files, and returns the chosen
//  StartupDownloadResult.
//
////////////////////////////////////////////////////////////////////////////////

StartupDownloadResult StartupDownloadDialog::Show (HINSTANCE                hInstance,
                                                   HWND                     hwndOwner,
                                                   std::string_view         themeName,
                                                   const std::wstring     & machineDisplayName,
                                                   StartupDownloadSet     & set)
{
    constexpr int  s_kLineHeightDip = 20;
    constexpr int  s_kIntroPadDip   = 8;
    constexpr int  s_kChromeDip     = 120;   // caption + content pad*2 + button row

    CassoTheme                             theme        = CassoTheme::ForName (std::string (themeName));
    DialogState                            state;
    DownloadDialog                         dlg;
    std::unique_ptr<DownloadContentPanel>  content      = std::make_unique<DownloadContentPanel>();
    DxuiLabel                              introLabel;
    DownloadBodyPanel                      body;
    DxuiWindow::CreateParams               params;
    std::wstring                           title;
    std::wstring                           intro;
    std::wstring                           prevGroup;
    UINT                                   sysDpi       = (hwndOwner != nullptr) ? GetDpiForWindow (hwndOwner)
                                                                                 : GetDpiForSystem();
    bool                                   requiresRoms = false;
    int                                    rowCount     = 0;
    int                                    headerCount  = 0;
    int                                    introLines   = 1;
    int                                    heightDip    = 0;


    if (set.Empty())
    {
        return StartupDownloadResult::NothingToDo;
    }

    state.set        = &set;
    state.runtime    = std::vector<EntryRuntime> (set.entries.size());
    state.checkboxes = std::vector<DxuiCheckbox> (set.entries.size());
    state.dpi        = sysDpi;
    requiresRoms     = set.RequiresRoms();
    rowCount         = (int) set.entries.size();

    for (size_t i = 0; i < set.entries.size(); i++)
    {
        DxuiCheckbox       & cb    = state.checkboxes[i];
        StartupAssetEntry  & entry = set.entries[i];

        cb.SetDpi      (sysDpi);
        cb.SetLabel    (entry.displayName);
        cb.SetChecked  (entry.selected);
        cb.SetEnabled  (entry.selectable);
        cb.SetOnChange ([&entry] (bool checked) { entry.selected = checked; });
    }

    for (const StartupAssetEntry & entry : set.entries)
    {
        if (entry.groupLabel != prevGroup)
        {
            headerCount++;
            prevGroup = entry.groupLabel;
        }
    }

    title  = L"Casso ";
    title += s_kchEmDash;
    title += L" Download assets";

    if (requiresRoms)
    {
        std::wstring  machineStr = machineDisplayName.empty() ? L"" : machineDisplayName;

        intro  = L"The ";
        intro += machineStr;
        intro += L" needs the following files to boot.\n"
                 L"Click Download to fetch them now, or Exit to quit.";
    }
    else
    {
        intro = L"The following optional files are missing. "
                L"Choose what to download, then click Download,\n"
                L"Skip to continue without them, or Exit to quit.";
    }

    for (wchar_t ch : intro)
    {
        if (ch == L'\n')
        {
            introLines++;
        }
    }

    introLabel.SetText      (intro);
    introLabel.SetColorArgb (theme.bodyText);
    introLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    body.SetPaintFn ([&set, &state] (DialogPaintContext & ctx) { PaintBody (ctx, set, state); });
    body.SetInputFn ([&state] (const DialogInputEvent & ev) { (void) HandleBodyInput (ev, state); });

    content->Init (&introLabel, &body, introLines * s_kLineHeightDip + s_kIntroPadDip);

    heightDip = s_kChromeDip + introLines * s_kLineHeightDip + s_kIntroPadDip
              + headerCount * (s_kHeaderHeightDp + s_kRowGapDp + s_kHeaderGapAboveDp)
              + rowCount    * (s_kRowHeightDp    + s_kRowGapDp);

    dlg.ConfigureDownload (std::move (content), requiresRoms, s_kTickIntervalMs);

    params.title                    = title;
    params.hInstance                = hInstance;
    params.ownerHwnd                = hwndOwner;
    params.initialSizeDip           = { s_kBodyWidthDp, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    if (FAILED (dlg.Create (params)))
    {
        return StartupDownloadResult::Exit;
    }

    dlg.SetTheme (&theme);

    //  Download starts the workers in place (the button stays open and
    //  relabels); Skip auto-closes with the default Skipped result; Exit
    //  (the IDCANCEL / close-box button) records the Exit result. A fast
    //  poll closes the dialog once every worker has finished.
    dlg.SetOnDownloadClick ([&state, &dlg] ()
    {
        if (!state.downloading)
        {
            state.downloading = true;
            state.showStatus  = true;
            dlg.DownloadButton()->SetLabel   (L"Downloading...");
            dlg.DownloadButton()->SetEnabled (false);

            if (dlg.SkipButton() != nullptr)
            {
                dlg.SkipButton()->SetVisible (false);
            }

            StartWorkers (state);
            dlg.Invalidate();
        }
    });

    dlg.SetOnPoll ([&state, &dlg] ()
    {
        if (state.downloading
            && !state.finished
            && state.workersInFlight.load (std::memory_order_acquire) == 0)
        {
            state.finished = true;
            JoinAllWorkers (state);
            RemovePartialFiles (state);

            state.result = state.anyFailed.load (std::memory_order_relaxed)
                              ? StartupDownloadResult::PartialDone
                              : StartupDownloadResult::AllDone;

            dlg.EndDialog ((int) state.result);
        }
    });

    if (dlg.ExitButton() != nullptr)
    {
        dlg.ExitButton()->SetOnClick ([&state, &dlg] ()
        {
            state.result = StartupDownloadResult::Exit;
            dlg.EndDialog (IDCANCEL);
        });
    }

    (void) dlg.ShowDialog (s_kIdDownload);

    if (!state.workers.empty())
    {
        state.cancelFlag.store (true, std::memory_order_release);
        JoinAllWorkers (state);
        RemovePartialFiles (state);

        if (state.result == StartupDownloadResult::Skipped)
        {
            state.result = StartupDownloadResult::Exit;
        }
    }

    return state.result;
}
