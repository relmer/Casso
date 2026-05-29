#include "Pch.h"

#include "StartupDownloadDialog.h"

#include "DialogPrimitive.h"
#include "StandaloneDialog.h"
#include "../Chrome/ChromeTheme.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Checkbox.h"
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




bool StartupDownloadSet::RequiresRoms () const
{
    for (const StartupAssetEntry & entry : entries)
    {
        if (entry.kind == StartupAssetKind::Rom)
        {
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  StartupDownloadDialog::Show
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    enum class EntryStatus
    {
        Pending,
        Downloading,
        Done,
        Failed,
        Cancelled,
        Skipped
    };



    struct EntryRuntime
    {
        std::atomic<std::uint64_t>  bytesDone{0};
        std::atomic<int>            status{(int) EntryStatus::Pending};
        std::string                 errorMsg;
        bool                        startedWrite = false;
    };



    struct DialogState
    {
        StartupDownloadSet  *        set         = nullptr;
        std::vector<EntryRuntime>    runtime;
        std::vector<Checkbox>        checkboxes;     // parallel to entries
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



    void WorkerThreadProc (DialogState * state, size_t index)
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



    void StartWorkers (DialogState & state)
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



    void JoinAllWorkers (DialogState & state)
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



    void RemovePartialFiles (DialogState & state)
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



    std::wstring StatusText (const EntryRuntime & rt, std::uint64_t expected)
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
}




StartupDownloadResult StartupDownloadDialog::Show (HINSTANCE                hInstance,
                                                   HWND                     hwndOwner,
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
    state.checkboxes = std::vector<Checkbox> (set.entries.size());
    state.dpi        = sysDpi;
    requiresRoms     = set.RequiresRoms();
    rowCount         = (int) set.entries.size();

    for (size_t i = 0; i < set.entries.size(); i++)
    {
        Checkbox          & cb    = state.checkboxes[i];
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
        if (machineDisplayName.empty())
        {
            intro = L"This machine configuration needs the following files to boot. "
                    L"Click Download to fetch them now, or Exit to quit.";
        }
        else
        {
            intro  = L"The ";
            intro += machineDisplayName;
            intro += L" configuration needs the following files to boot. "
                     L"Click Download to fetch them now, or Exit to quit.";
        }
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

    // Paint hook: single-line tree rows. Group headers are bold no-row;
    // each entry row paints a Checkbox widget on the left, then a dim
    // source label, then (when status is showing) a right-aligned
    // percent / Done / Failed indicator.
    def.onPaintCustomBody = [&set, &state] (DialogPaintContext & ctx)
    {
        HRESULT   hr        = S_OK;
        float     x         = 0.0f;
        float     y         = 0.0f;
        float     fullW     = 0.0f;
        float     rowH      = (float) s_kRowHeightDp        * ctx.dpiScale;
        float     headerH   = (float) s_kHeaderHeightDp     * ctx.dpiScale;
        float     headerGap = (float) s_kHeaderGapAboveDp   * ctx.dpiScale;
        float     gap       = (float) s_kRowGapDp           * ctx.dpiScale;
        float     fontPx    = s_kFontDp                     * ctx.dpiScale;
        float     hFontPx   = s_kHeaderFontDp               * ctx.dpiScale;
        float     sourceW   = s_kSourceColumnDp             * ctx.dpiScale;
        float     statusW   = s_kStatusColumnDp             * ctx.dpiScale;
        float     colGap    = s_kColumnGapDp                * ctx.dpiScale;
        uint32_t  fg        = 0;
        uint32_t  fgDim     = 0;
        uint32_t  hdrFg     = 0;
        wstring   curGroup;



        if (ctx.painter == nullptr || ctx.text == nullptr || ctx.theme == nullptr)
        {
            return;
        }

        x       = (float) ctx.customBodyRect.left;
        y       = (float) ctx.customBodyRect.top;
        fullW   = (float) (ctx.customBodyRect.right - ctx.customBodyRect.left);
        fg      = ctx.theme->dropdownItemTextArgb;
        fgDim   = (fg & 0x00FFFFFFu) | 0x70000000u;
        hdrFg   = ctx.theme->titleTextArgb;

        state.bodyOriginXPx = ctx.customBodyRect.left;
        state.bodyOriginYPx = ctx.customBodyRect.top;

        for (size_t i = 0; i < set.entries.size(); i++)
        {
            const StartupAssetEntry & entry  = set.entries[i];
            const EntryRuntime      & rt     = state.runtime[i];
            Checkbox                & cb     = state.checkboxes[i];
            std::wstring              status = state.showStatus
                                                  ? StatusText (rt, entry.expectedBytes)
                                                  : wstring();

            if (entry.groupLabel != curGroup)
            {
                if (!curGroup.empty())
                {
                    y += headerGap;
                }

                curGroup = entry.groupLabel;

                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (curGroup.c_str(),
                                                               x, y,
                                                               fullW, headerH,
                                                               hdrFg, hFontPx, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Left,
                                                               DwriteTextRenderer::VAlign::Center,
                                                               DWRITE_FONT_WEIGHT_BOLD));

                y += headerH + gap;
            }

            float  ry         = y;
            float  cbAvailW   = fullW - sourceW - statusW - colGap * 2.0f;
            RECT   cbRect     = { (LONG) x,
                                  (LONG) ry,
                                  (LONG) (x + cbAvailW),
                                  (LONG) (ry + rowH) };

            cb.SetChecked (entry.selected);
            cb.SetEnabled (entry.selectable && !state.downloading);
            cb.SetRect    (cbRect);
            cb.SetLabel   (entry.displayName);
            cb.Paint      (*ctx.painter, *ctx.text);

            // Source column (dim).
            IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (entry.source.c_str(),
                                                           x + cbAvailW + colGap, ry,
                                                           sourceW, rowH,
                                                           fgDim, fontPx, L"Segoe UI",
                                                           DwriteTextRenderer::HAlign::Left,
                                                           DwriteTextRenderer::VAlign::Center));

            // Status column (right-aligned). Same fg as label -- no red.
            if (state.showStatus && entry.selected)
            {
                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (status.c_str(),
                                                               x + fullW - statusW, ry,
                                                               statusW, rowH,
                                                               fg, fontPx, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Right,
                                                               DwriteTextRenderer::VAlign::Center));
            }

            y += rowH + gap;
        }
    };

    // Input hook: forward mouse events to the per-row Checkbox widgets.
    // Coordinates arrive body-relative; the Checkbox widget hit-tests
    // against absolute window coordinates (the same space its rect is
    // set to during paint), so we add the cached body origin.
    def.onInputCustomBody = [&state] (const DialogInputEvent & ev) -> std::optional<int>
    {
        if (state.downloading)
        {
            return std::nullopt;
        }

        int absX = ev.xPx + state.bodyOriginXPx;
        int absY = ev.yPx + state.bodyOriginYPx;

        for (Checkbox & cb : state.checkboxes)
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

    def.onButtonActivated = [&state, hInstance] (size_t idx, DialogPrimitive & dlg) -> bool
    {
        UNREFERENCED_PARAMETER (hInstance);

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
    };

    def.onTick = [&state] (DialogPrimitive & dlg)
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
    };

    dialogResult = ShowStandaloneDialog (hInstance, hwndOwner, def);
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
