#include "Pch.h"

#include "StartupDownloadDialog.h"

#include "DialogPrimitive.h"
#include "StandaloneDialog.h"
#include "../Chrome/ChromeTheme.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "../../UnicodeSymbols.h"


namespace fs = std::filesystem;


static constexpr int    s_kRowHeightDp        = 28;
static constexpr int    s_kRowGapDp           = 2;
static constexpr int    s_kBodyWidthDp        = 560;
static constexpr int    s_kHeaderHeightDp     = 26;
static constexpr int    s_kHeaderGapAboveDp   = 6;
static constexpr float  s_kFontDp             = 13.0f;
static constexpr float  s_kHeaderFontDp       = 13.0f;
static constexpr float  s_kSourceColumnDp     = 170.0f;
static constexpr float  s_kStatusColumnDp     = 64.0f;
static constexpr float  s_kColumnGapDp        = 12.0f;
static constexpr unsigned int  s_kTickIntervalMs = 100;

static constexpr int    s_kIdDownload = 100;
static constexpr int    s_kIdSkip     = 101;
static constexpr int    s_kIdExit     = 102;





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
    size_t                       downloadBtnIdx = SIZE_MAX;
    size_t                       skipBtnIdx     = SIZE_MAX;
    size_t                       exitBtnIdx     = SIZE_MAX;
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
    cb.Paint      (*ctx.painter, *ctx.text);

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
    fg    = ctx.theme->dropdownItemTextArgb;
    fgDim = (fg & 0x00FFFFFFu) | 0x70000000u;
    hdrFg = ctx.theme->titleTextArgb;

    state.bodyOriginXPx = ctx.customBodyRect.left;
    state.bodyOriginYPx = ctx.customBodyRect.top;

    hdrLabel.SetDpi         (state.dpi);
    hdrLabel.SetFontSizeDip (s_kHeaderFontDp);
    hdrLabel.SetColorArgb   (hdrFg);
    hdrLabel.SetFontWeight  (DWRITE_FONT_WEIGHT_BOLD);

    sourceLabel.SetDpi         (state.dpi);
    sourceLabel.SetFontSizeDip (s_kFontDp);
    sourceLabel.SetColorArgb   (fgDim);

    statusLabel.SetDpi         (state.dpi);
    statusLabel.SetFontSizeDip (s_kFontDp);
    statusLabel.SetColorArgb   (fg);
    statusLabel.SetHAlign      (DxuiTextRenderer::HAlign::Right);

    for (size_t i = 0; i < set.entries.size(); i++)
    {
        const StartupAssetEntry & entry  = set.entries[i];
        const EntryRuntime      & rt     = state.runtime[i];
        std::wstring              status = state.showStatus
                                              ? StatusText (rt, entry.expectedBytes)
                                              : wstring();

        if (entry.groupLabel != curGroup)
        {
            if (!curGroup.empty())
            {
                y += m.headerGap;
            }

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





////////////////////////////////////////////////////////////////////////////////
//
//  HandleButtonActivated
//
//  Returns true to close the dialog, false to keep it open. Download
//  swaps the dialog into "downloading" mode and spins up workers; Skip /
//  Exit set the result and signal close (cancelling and joining workers
//  first on Exit).
//
////////////////////////////////////////////////////////////////////////////////

bool StartupDownloadDialog::HandleButtonActivated (size_t idx, DialogPrimitive & dlg, DialogState & state)
{
    if (idx == state.downloadBtnIdx)
    {
        if (state.downloading)
        {
            return false;
        }

        state.downloading = true;
        state.showStatus  = true;
        dlg.SetButtonLabel   (state.downloadBtnIdx, L"Downloading...");
        dlg.SetButtonEnabled (state.downloadBtnIdx, false);

        if (state.skipBtnIdx != SIZE_MAX)
        {
            dlg.SetButtonVisible (state.skipBtnIdx, false);
        }

        StartWorkers (state);
        return false;
    }

    if (state.skipBtnIdx != SIZE_MAX && idx == state.skipBtnIdx)
    {
        state.result = StartupDownloadResult::Skipped;
        return true;
    }

    if (idx == state.exitBtnIdx)
    {
        if (state.downloading)
        {
            state.cancelFlag.store (true, std::memory_order_release);
            JoinAllWorkers (state);
            RemovePartialFiles (state);
        }

        state.result = StartupDownloadResult::Exit;
        return true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleTick
//
//  Periodic repaint plus completion detection. When workers are all
//  done, finalizes result and closes the dialog.
//
////////////////////////////////////////////////////////////////////////////////

void StartupDownloadDialog::HandleTick (DialogPrimitive & dlg, DialogState & state)
{
    dlg.Repaint();

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

        dlg.Close ((int) state.result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDownloadDialog::Show
//
//  Constructs and modally displays the unified startup-asset download
//  dialog. Builds checkbox widgets parallel to set.entries, wires the
//  custom-body paint/input hooks, button activation, and tick handlers
//  to free helpers (PaintBody / HandleBodyInput / HandleButtonActivated /
//  HandleTick), and runs the standalone dialog loop. On exit, cancels
//  and joins any still-running download workers, scrubs partial files,
//  and returns the chosen StartupDownloadResult.
//
//  `themeName` is forwarded to ShowStandaloneDialog so the dialog
//  honours the user's persisted ChromeTheme choice; the caller passes
//  GlobalUserPrefs::activeTheme.
//
////////////////////////////////////////////////////////////////////////////////

StartupDownloadResult StartupDownloadDialog::Show (HINSTANCE                hInstance,
                                                   HWND                     hwndOwner,
                                                   std::string_view         themeName,
                                                   const std::wstring     & machineDisplayName,
                                                   StartupDownloadSet     & set)
{
    DialogDefinition        def           = {};
    DialogState             state;
    wstring                 title;
    wstring                 intro;
    int                     dialogResult  = 0;
    UINT                    sysDpi        = (hwndOwner != nullptr) ? GetDpiForWindow (hwndOwner)
                                                                   : GetDpiForSystem();
    float                   dpiScale      = (sysDpi > 0) ? ((float) sysDpi / 96.0f) : 1.0f;
    bool                    requiresRoms  = false;
    int                     rowCount      = 0;
    int                     headerCount   = 0;
    int                     totalH        = 0;
    wstring                 prevGroup;



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
        DxuiCheckbox          & cb    = state.checkboxes[i];
        StartupAssetEntry & entry = set.entries[i];

        cb.SetDpi     (sysDpi);
        cb.SetLabel   (entry.displayName);
        cb.SetChecked (entry.selected);
        cb.SetEnabled (entry.selectable);
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
        wstring machineStr = machineDisplayName.empty() ? L"" : machineDisplayName;

        intro  = L"The ";
        intro += machineStr;
        intro += L" needs the following files to boot.\n"
                 L"Click Download to fetch them now, or Exit to quit.";
    }
    else
    {
        intro = L"The following optional files are missing. "
                L"Choose what to download, then click Download, "
                L"Skip to continue without them, or Exit to quit.";
    }

    totalH = (int) ((float) (headerCount * (s_kHeaderHeightDp + s_kRowGapDp + s_kHeaderGapAboveDp)
                             + rowCount  * (s_kRowHeightDp    + s_kRowGapDp))
                    * dpiScale);

    def.title              = title;
    def.icon               = DialogIcon::AppFlat;
    def.iconSizeOverrideDp = 64.0f;
    def.body.push_back ({ intro, false, L"" });

    def.customBodyMinSizePx.cx = (int) ((float) s_kBodyWidthDp * dpiScale);
    def.customBodyMinSizePx.cy = totalH;

    def.tickIntervalMs = s_kTickIntervalMs;

    def.onPaintCustomBody = [&set, &state] (DialogPaintContext & ctx)
    {
        PaintBody (ctx, set, state);
    };

    def.onInputCustomBody = [&state] (const DialogInputEvent & ev, DialogPrimitive &) -> std::optional<int>
    {
        return HandleBodyInput (ev, state);
    };

    // Build buttons.
    state.downloadBtnIdx = def.buttons.size();
    def.buttons.push_back ({ L"Download", s_kIdDownload, true, false });

    if (!requiresRoms)
    {
        state.skipBtnIdx = def.buttons.size();
        def.buttons.push_back ({ L"Skip", s_kIdSkip, false, false });
    }

    state.exitBtnIdx = def.buttons.size();
    def.buttons.push_back ({ L"Exit", s_kIdExit, false, true });

    def.onButtonActivated = [&state] (size_t idx, DialogPrimitive & dlg) -> bool
    {
        return HandleButtonActivated (idx, dlg, state);
    };

    def.onTick = [&state] (DialogPrimitive & dlg)
    {
        HandleTick (dlg, state);
    };

    dialogResult = ShowStandaloneDialog (hInstance, hwndOwner, themeName, def);
    UNREFERENCED_PARAMETER (dialogResult);

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
