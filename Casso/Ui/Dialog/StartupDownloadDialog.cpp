#include "Pch.h"

#include "StartupDownloadDialog.h"

#include "DialogPrimitive.h"
#include "StandaloneDialog.h"
#include "../Chrome/ChromeTheme.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../../UnicodeSymbols.h"


namespace fs = std::filesystem;


static constexpr int    s_kRowHeightDp        = 56;
static constexpr int    s_kRowGapDp           = 4;
static constexpr int    s_kBodyWidthDp        = 540;
static constexpr int    s_kHeaderHeightDp     = 26;
static constexpr float  s_kFontDp             = 13.0f;
static constexpr float  s_kHeaderFontDp       = 13.0f;
static constexpr float  s_kTextPaddingDp      = 8.0f;
static constexpr float  s_kProgressBarHeightDp = 8.0f;
static constexpr float  s_kStatusColumnDp     = 160.0f;
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
        Cancelled
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
        std::atomic<bool>            cancelFlag{false};
        std::atomic<bool>            workerDone {false};
        std::atomic<bool>            anyFailed  {false};
        std::thread                  worker;
        size_t                       downloadBtnIdx = SIZE_MAX;
        size_t                       skipBtnIdx     = SIZE_MAX;
        size_t                       exitBtnIdx     = SIZE_MAX;
        bool                         downloading    = false;
        bool                         showStatus     = false;  // gates per-row status text
        StartupDownloadResult        result         = StartupDownloadResult::Skipped;
    };



    std::wstring FormatBytes (std::uint64_t bytes)
    {
        wchar_t buf[64] = {};

        if (bytes < 1024ull)
        {
            swprintf_s (buf, L"%llu B", (unsigned long long) bytes);
        }
        else if (bytes < 1024ull * 1024ull)
        {
            swprintf_s (buf, L"%.1f KB", (double) bytes / 1024.0);
        }
        else
        {
            swprintf_s (buf, L"%.2f MB", (double) bytes / (1024.0 * 1024.0));
        }

        return buf;
    }



    std::wstring StatusText (const EntryRuntime & rt, std::uint64_t expected)
    {
        EntryStatus    s        = (EntryStatus) rt.status.load (std::memory_order_relaxed);
        std::uint64_t  done     = rt.bytesDone.load (std::memory_order_relaxed);
        std::wstring   doneStr  = FormatBytes (done);
        std::wstring   totalStr = (expected > 0) ? FormatBytes (expected) : std::wstring (L"?");

        switch (s)
        {
            case EntryStatus::Pending:     return L"Waiting...";
            case EntryStatus::Downloading: return doneStr + L" / " + totalStr;
            case EntryStatus::Done:        return L"Done";
            case EntryStatus::Failed:      return L"Failed";
            case EntryStatus::Cancelled:   return L"Cancelled";
        }

        return L"";
    }



    void WorkerThreadProc (DialogState * state)
    {
        if (state == nullptr || state->set == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < state->set->entries.size(); i++)
        {
            StartupAssetEntry & entry = state->set->entries[i];
            EntryRuntime      & rt    = state->runtime[i];

            if (!entry.selected)
            {
                // User unchecked this asset. Leave runtime untouched so
                // the row shows blank status (handled by paint).
                continue;
            }

            if (state->cancelFlag.load (std::memory_order_relaxed))
            {
                rt.status.store ((int) EntryStatus::Cancelled, std::memory_order_relaxed);
                continue;
            }

            rt.status.store ((int) EntryStatus::Downloading, std::memory_order_relaxed);
            rt.startedWrite = true;

            HRESULT  hr = S_OK;

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
        }

        state->workerDone.store (true, std::memory_order_release);
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

    state.set     = &set;
    state.runtime = std::vector<EntryRuntime> (set.entries.size());
    requiresRoms  = set.RequiresRoms();
    rowCount      = (int) set.entries.size();

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

    totalH = (int) ((float) (s_kHeaderHeightDp
                             + headerCount * (s_kHeaderHeightDp + s_kRowGapDp)
                             + rowCount    * (s_kRowHeightDp    + s_kRowGapDp))
                    * dpiScale);

    def.title              = title;
    def.icon               = DialogIcon::AppFlat;
    def.iconSizeOverrideDp = 64.0f;
    def.body.push_back ({ intro, false, L"" });

    def.customBodyMinSizePx.cx = (int) ((float) s_kBodyWidthDp * dpiScale);
    def.customBodyMinSizePx.cy = totalH;

    def.tickIntervalMs = s_kTickIntervalMs;

    // Paint hook: tree-style layout. Group headers are bold no-checkbox
    // rows; entries are checkbox + label + progress bar + (optional)
    // status. Status is hidden until the user clicks Download.
    def.onPaintCustomBody = [&set, &state] (DialogPaintContext & ctx)
    {
        HRESULT   hr        = S_OK;
        float     x         = 0.0f;
        float     y         = 0.0f;
        float     fullW     = 0.0f;
        float     rowH      = (float) s_kRowHeightDp        * ctx.dpiScale;
        float     headerH   = (float) s_kHeaderHeightDp     * ctx.dpiScale;
        float     gap       = (float) s_kRowGapDp           * ctx.dpiScale;
        float     fontPx    = s_kFontDp                     * ctx.dpiScale;
        float     hFontPx   = s_kHeaderFontDp               * ctx.dpiScale;
        float     pad       = s_kTextPaddingDp              * ctx.dpiScale;
        float     barH      = s_kProgressBarHeightDp        * ctx.dpiScale;
        float     statusW   = s_kStatusColumnDp             * ctx.dpiScale;
        float     cbBoxDp   = 16.0f;
        float     cbBoxPx   = cbBoxDp                       * ctx.dpiScale;
        float     indentPx  = 20.0f                         * ctx.dpiScale;
        float     cbColPx   = indentPx + cbBoxPx + pad;
        uint32_t  rowBgA    = 0;
        uint32_t  rowBgB    = 0;
        uint32_t  fg        = 0;
        uint32_t  fgDim     = 0;
        uint32_t  hdrFg     = 0;
        uint32_t  barBg     = 0;
        uint32_t  barFg     = 0;
        uint32_t  barDone   = 0;
        uint32_t  barFail   = 0;
        wstring   curGroup;
        size_t    rowIdx    = 0;



        if (ctx.painter == nullptr || ctx.text == nullptr || ctx.theme == nullptr)
        {
            return;
        }

        x       = (float) ctx.customBodyRect.left;
        y       = (float) ctx.customBodyRect.top;
        fullW   = (float) (ctx.customBodyRect.right - ctx.customBodyRect.left);
        rowBgA  = ctx.theme->navStripArgb;
        rowBgB  = ctx.theme->dropdownBgArgb;
        fg      = ctx.theme->dropdownItemTextArgb;
        fgDim   = (fg & 0x00FFFFFFu) | 0x80000000u;
        hdrFg   = ctx.theme->titleTextArgb;
        barBg   = ctx.theme->dropdownHoverArgb;
        barFg   = ctx.theme->navHoverArgb;
        barDone = ctx.theme->ledActiveArgb;
        barFail = 0xFFCC4444;

        for (size_t i = 0; i < set.entries.size(); i++)
        {
            const StartupAssetEntry & entry  = set.entries[i];
            const EntryRuntime      & rt     = state.runtime[i];
            EntryStatus               s      = (EntryStatus) rt.status.load (std::memory_order_relaxed);
            std::uint64_t             done   = rt.bytesDone.load (std::memory_order_relaxed);

            if (entry.groupLabel != curGroup)
            {
                curGroup = entry.groupLabel;

                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (curGroup.c_str(),
                                                               x + pad, y,
                                                               fullW - pad * 2.0f, headerH,
                                                               hdrFg, hFontPx, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Left,
                                                               DwriteTextRenderer::VAlign::Center,
                                                               DWRITE_FONT_WEIGHT_BOLD));

                y += headerH + gap;
            }

            float       ry        = y;
            uint32_t    rowBg     = ((rowIdx & 1u) == 0u) ? rowBgA : rowBgB;
            std::wstring  status  = state.showStatus
                                       ? StatusText (rt, entry.expectedBytes)
                                       : wstring();
            uint32_t    rowFg     = entry.selected ? fg : fgDim;

            ctx.painter->FillRect (x, ry, fullW, rowH, rowBg);

            // Checkbox.
            {
                float     cx     = x + indentPx;
                float     cy     = ry + (rowH - cbBoxPx) * 0.5f;
                uint32_t  edge   = entry.selectable ? fg : fgDim;
                uint32_t  fill   = entry.selected   ? barDone : 0x00000000u;

                ctx.painter->FillRect (cx,            cy,            cbBoxPx, 1.0f * ctx.dpiScale, edge);
                ctx.painter->FillRect (cx,            cy + cbBoxPx - 1.0f * ctx.dpiScale, cbBoxPx, 1.0f * ctx.dpiScale, edge);
                ctx.painter->FillRect (cx,            cy,            1.0f * ctx.dpiScale, cbBoxPx, edge);
                ctx.painter->FillRect (cx + cbBoxPx - 1.0f * ctx.dpiScale, cy,            1.0f * ctx.dpiScale, cbBoxPx, edge);

                if (entry.selected)
                {
                    float inset = 3.0f * ctx.dpiScale;
                    ctx.painter->FillRect (cx + inset, cy + inset,
                                           cbBoxPx - inset * 2.0f, cbBoxPx - inset * 2.0f, fill);
                }
            }

            IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (entry.displayName.c_str(),
                                                           x + cbColPx, ry,
                                                           fullW - cbColPx - statusW - pad, rowH * 0.55f,
                                                           rowFg, fontPx, L"Segoe UI",
                                                           DwriteTextRenderer::HAlign::Left,
                                                           DwriteTextRenderer::VAlign::Center));

            if (state.showStatus)
            {
                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (status.c_str(),
                                                               x + fullW - statusW - pad, ry,
                                                               statusW, rowH * 0.55f,
                                                               rowFg, fontPx, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Right,
                                                               DwriteTextRenderer::VAlign::Center));
            }

            if (state.showStatus && entry.selected)
            {
                float    barX     = x + cbColPx;
                float    barY     = ry + rowH * 0.55f + pad * 0.5f;
                float    barFullW = fullW - cbColPx - pad;
                float    pct      = 0.0f;
                uint32_t fillCol  = barFg;

                if (s == EntryStatus::Done)
                {
                    pct     = 1.0f;
                    fillCol = barDone;
                }
                else if (s == EntryStatus::Failed)
                {
                    pct     = 1.0f;
                    fillCol = barFail;
                }
                else if (s == EntryStatus::Cancelled)
                {
                    pct = 0.0f;
                }
                else if (entry.expectedBytes > 0 && done > 0)
                {
                    pct = (float) ((double) done / (double) entry.expectedBytes);

                    if (pct > 1.0f)
                    {
                        pct = 1.0f;
                    }
                }
                else if (s == EntryStatus::Downloading)
                {
                    pct = 0.25f;
                }

                ctx.painter->FillRect (barX, barY, barFullW, barH, barBg);

                if (pct > 0.0f)
                {
                    ctx.painter->FillRect (barX, barY, barFullW * pct, barH, fillCol);
                }
            }

            if (state.showStatus && s == EntryStatus::Failed && !rt.errorMsg.empty())
            {
                std::wstring  msg;

                msg.reserve (rt.errorMsg.size());

                for (char ch : rt.errorMsg)
                {
                    msg.push_back (static_cast<wchar_t> ((unsigned char) ch));
                }

                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (msg.c_str(),
                                                               x + cbColPx, ry + rowH - pad * 1.5f,
                                                               fullW - cbColPx - pad, pad * 1.5f,
                                                               barFail, fontPx * 0.85f, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Left,
                                                               DwriteTextRenderer::VAlign::Center));
            }

            y += rowH + gap;
            rowIdx++;
        }
    };

    // Input hook: toggle entry.selected when the user clicks within a
    // row's checkbox (while not yet downloading). Coordinates are
    // already relative to the custom-body rect.
    def.onInputCustomBody = [&set, &state, dpiScale] (
        const DialogInputEvent & ev) -> std::optional<int>
    {
        if (ev.kind != DialogInputEvent::Kind::LeftButtonDown)
        {
            return std::nullopt;
        }

        if (state.downloading)
        {
            return std::nullopt;
        }

        float    rowH      = (float) s_kRowHeightDp    * dpiScale;
        float    headerH   = (float) s_kHeaderHeightDp * dpiScale;
        float    gap       = (float) s_kRowGapDp       * dpiScale;
        float    indentPx  = 20.0f                     * dpiScale;
        float    cbBoxPx   = 16.0f                     * dpiScale;
        float    pad       = s_kTextPaddingDp          * dpiScale;
        float    cbColPx   = indentPx + cbBoxPx + pad;
        float    y         = 0.0f;
        wstring  curGroup;

        for (size_t i = 0; i < set.entries.size(); i++)
        {
            StartupAssetEntry & entry = set.entries[i];

            if (entry.groupLabel != curGroup)
            {
                curGroup = entry.groupLabel;
                y       += headerH + gap;
            }

            float ry = y;
            y       += rowH + gap;

            if ((float) ev.yPx < ry || (float) ev.yPx >= ry + rowH)
            {
                continue;
            }

            if (!entry.selectable)
            {
                return std::nullopt;
            }

            if ((float) ev.xPx < cbColPx)
            {
                entry.selected = !entry.selected;
                return std::nullopt;
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

            state.worker = std::thread (WorkerThreadProc, &state);
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

                if (state.worker.joinable())
                {
                    state.worker.join();
                }

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

        if (state.downloading && state.workerDone.load (std::memory_order_acquire))
        {
            if (state.worker.joinable())
            {
                state.worker.join();
            }

            RemovePartialFiles (state);

            state.result = state.anyFailed.load (std::memory_order_relaxed)
                              ? StartupDownloadResult::PartialDone
                              : StartupDownloadResult::AllDone;

            dlg.Close ((int) state.result);
        }
    };

    dialogResult = ShowStandaloneDialog (hInstance, hwndOwner, def);
    UNREFERENCED_PARAMETER (dialogResult);

    if (state.worker.joinable())
    {
        state.cancelFlag.store (true, std::memory_order_release);
        state.worker.join();
        RemovePartialFiles (state);

        if (state.result == StartupDownloadResult::Skipped)
        {
            state.result = StartupDownloadResult::Exit;
        }
    }

    return state.result;
}
