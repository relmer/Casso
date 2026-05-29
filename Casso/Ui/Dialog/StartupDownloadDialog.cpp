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
                                                   StartupDownloadSet     & set)
{
    DialogDefinition        def           = {};
    DialogState             state;
    wstring                 title;
    wstring                 intro;
    wstring                 sources;
    int                     dialogResult  = 0;
    UINT                    sysDpi        = (hwndOwner != nullptr) ? GetDpiForWindow (hwndOwner)
                                                                   : GetDpiForSystem();
    float                   dpiScale      = (sysDpi > 0) ? ((float) sysDpi / 96.0f) : 1.0f;
    bool                    requiresRoms  = false;
    int                     rowCount      = 0;
    int                     totalH        = 0;



    if (set.Empty())
    {
        return StartupDownloadResult::NothingToDo;
    }

    state.set     = &set;
    state.runtime = std::vector<EntryRuntime> (set.entries.size());
    requiresRoms  = set.RequiresRoms();
    rowCount      = (int) set.entries.size();

    title  = L"Casso ";
    title += s_kchEmDash;
    title += L" Download assets";

    if (requiresRoms)
    {
        intro = L"Casso needs the assets listed below to boot. "
                L"Click Download to fetch them now, or Exit to quit.";
    }
    else
    {
        intro = L"The following optional assets are missing. "
                L"Click Download to fetch them, Skip to continue without them, "
                L"or Exit to quit.";
    }

    // Build the sources blurb so the user knows these files come from
    // third parties and are not bundled with Casso.
    {
        bool  hasRom    = false;
        bool  hasAudio  = false;
        bool  hasDisk   = false;

        for (const StartupAssetEntry & entry : set.entries)
        {
            switch (entry.kind)
            {
            case StartupAssetKind::Rom:        hasRom   = true; break;
            case StartupAssetKind::DriveAudio: hasAudio = true; break;
            case StartupAssetKind::BootDisk:   hasDisk  = true; break;
            }
        }

        sources  = L"These files are not bundled with Casso. They will be downloaded from:";
        if (hasRom)
        {
            sources += L"\n    \x2022  ROMs: the AppleWin project (raw.githubusercontent.com)";
        }
        if (hasAudio)
        {
            sources += L"\n    \x2022  Drive audio: OpenEmulator (raw.githubusercontent.com)";
        }
        if (hasDisk)
        {
            sources += L"\n    \x2022  Boot disks: Asimov mirror (apple.asimov.net)";
        }
    }

    totalH = (int) ((float) (s_kHeaderHeightDp
                             + rowCount * (s_kRowHeightDp + s_kRowGapDp))
                    * dpiScale);

    def.title              = title;
    def.icon               = DialogIcon::AppFlat;
    def.iconSizeOverrideDp = 64.0f;
    def.body.push_back ({ intro,   false, L"" });
    def.body.push_back ({ sources, false, L"" });

    def.customBodyMinSizePx.cx = (int) ((float) s_kBodyWidthDp * dpiScale);
    def.customBodyMinSizePx.cy = totalH;

    def.tickIntervalMs = s_kTickIntervalMs;

    // Paint hook reads atomic status/bytes for each entry every frame.
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
        uint32_t  rowBgA    = 0;
        uint32_t  rowBgB    = 0;
        uint32_t  fg        = 0;
        uint32_t  hdrBg     = 0;
        uint32_t  hdrFg     = 0;
        uint32_t  barBg     = 0;
        uint32_t  barFg     = 0;
        uint32_t  barDone   = 0;
        uint32_t  barFail   = 0;



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
        hdrBg   = ctx.theme->navHoverArgb;
        hdrFg   = ctx.theme->titleTextArgb;
        barBg   = ctx.theme->dropdownHoverArgb;
        barFg   = ctx.theme->navHoverArgb;
        barDone = ctx.theme->ledActiveArgb;
        barFail = 0xFFCC4444;

        ctx.painter->FillRect (x, y, fullW, headerH, hdrBg);

        IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (L"Asset",
                                                       x + pad, y,
                                                       fullW - statusW - pad * 2.0f, headerH,
                                                       hdrFg, hFontPx, L"Segoe UI",
                                                       DwriteTextRenderer::HAlign::Left,
                                                       DwriteTextRenderer::VAlign::Center,
                                                       DWRITE_FONT_WEIGHT_BOLD));

        IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (L"Status",
                                                       x + fullW - statusW - pad, y,
                                                       statusW, headerH,
                                                       hdrFg, hFontPx, L"Segoe UI",
                                                       DwriteTextRenderer::HAlign::Right,
                                                       DwriteTextRenderer::VAlign::Center,
                                                       DWRITE_FONT_WEIGHT_BOLD));

        y += headerH + gap;

        for (size_t i = 0; i < set.entries.size(); i++)
        {
            const StartupAssetEntry & entry  = set.entries[i];
            const EntryRuntime      & rt     = state.runtime[i];
            EntryStatus               s      = (EntryStatus) rt.status.load (std::memory_order_relaxed);
            std::uint64_t             done   = rt.bytesDone.load (std::memory_order_relaxed);
            float                     ry     = y + (float) i * (rowH + gap);
            uint32_t                  rowBg  = ((i & 1u) == 0u) ? rowBgA : rowBgB;
            std::wstring              status = StatusText (rt, entry.expectedBytes);
            std::wstring              line1  = entry.kindLabel.empty()
                                                  ? entry.displayName
                                                  : (entry.displayName + L"  \u2013  " + entry.kindLabel);

            ctx.painter->FillRect (x, ry, fullW, rowH, rowBg);

            IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (line1.c_str(),
                                                           x + pad, ry,
                                                           fullW - statusW - pad * 2.0f, rowH * 0.55f,
                                                           fg, fontPx, L"Segoe UI",
                                                           DwriteTextRenderer::HAlign::Left,
                                                           DwriteTextRenderer::VAlign::Center));

            IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (status.c_str(),
                                                           x + fullW - statusW - pad, ry,
                                                           statusW, rowH * 0.55f,
                                                           fg, fontPx, L"Segoe UI",
                                                           DwriteTextRenderer::HAlign::Right,
                                                           DwriteTextRenderer::VAlign::Center));

            {
                float    barX     = x + pad;
                float    barY     = ry + rowH * 0.55f + pad * 0.5f;
                float    barFullW = fullW - pad * 2.0f;
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
                    pct     = 0.0f;
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
                    // Indeterminate (unknown size): show partial fill so the
                    // user sees we're alive.
                    pct = 0.25f;
                }

                ctx.painter->FillRect (barX, barY, barFullW, barH, barBg);

                if (pct > 0.0f)
                {
                    ctx.painter->FillRect (barX, barY, barFullW * pct, barH, fillCol);
                }
            }

            if (s == EntryStatus::Failed && !rt.errorMsg.empty())
            {
                std::wstring  msg;

                msg.reserve (rt.errorMsg.size());

                for (char ch : rt.errorMsg)
                {
                    msg.push_back (static_cast<wchar_t> ((unsigned char) ch));
                }

                IGNORE_RETURN_VALUE (hr, ctx.text->DrawString (msg.c_str(),
                                                               x + pad, ry + rowH - pad * 1.5f,
                                                               fullW - pad * 2.0f, pad * 1.5f,
                                                               barFail, fontPx * 0.85f, L"Segoe UI",
                                                               DwriteTextRenderer::HAlign::Left,
                                                               DwriteTextRenderer::VAlign::Center));
            }
        }
    };

    // Build buttons. Indices captured into state so the tick / button
    // hooks can address them by symbolic name.
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

            // Clean up any partials from failed/cancelled entries so we
            // don't leave truncated files on disk.
            RemovePartialFiles (state);

            state.result = state.anyFailed.load (std::memory_order_relaxed)
                              ? StartupDownloadResult::PartialDone
                              : StartupDownloadResult::AllDone;

            dlg.Close ((int) state.result);
        }
    };

    dialogResult = ShowStandaloneDialog (hInstance, hwndOwner, def);
    UNREFERENCED_PARAMETER (dialogResult);

    // Defensive: if somehow we exit Show() with the worker still running
    // (WM_CLOSE path bypasses onButtonActivated), cancel and join.
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
