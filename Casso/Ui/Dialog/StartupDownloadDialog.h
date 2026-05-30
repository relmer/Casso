#pragma once

#include "Pch.h"


class    Checkbox;
class    DialogPrimitive;
class    Label;
struct   DialogInputEvent;
struct   DialogPaintContext;




////////////////////////////////////////////////////////////////////////////////
//
//  StartupDownloadDialog
//
//  Single themed DX dialog that downloads every asset Casso needs to
//  boot (missing ROMs, optional Disk II drive audio, ...) in one
//  unified user experience. The caller assembles a `StartupDownloadSet`
//  describing each missing asset and a closure that knows how to fetch
//  it; the dialog drives the downloads on a worker thread, paints live
//  per-asset progress, and lets the user Exit cleanly (cancelling any
//  in-flight work and removing partial files) at any point.
//
//  Button policy:
//    * If any ROM is missing  -> [Download] [Exit]      (no Skip; can't boot
//                                                        without ROMs)
//    * Otherwise              -> [Download] [Skip] [Exit]
//
//  After the user clicks Download, the Download button is relabelled
//  "Downloading..." and disabled, the Skip button is hidden, and Exit
//  remains active. Exit cancels in-flight downloads, deletes any
//  partial files, and returns `Exit`.
//
////////////////////////////////////////////////////////////////////////////////



enum class StartupAssetKind
{
    Rom,
    DriveAudio,
    BootDisk
};



struct StartupAssetEntry
{
    StartupAssetKind                  kind          = StartupAssetKind::Rom;
    std::wstring                      groupLabel;         // tree parent ("Apple //e ROMs")
    std::wstring                      displayName;        // tree leaf ("Apple //e ROM")
    std::wstring                      kindLabel;          // e.g. "ROM" / "Drive audio"
    std::wstring                      source;             // human-readable origin ("AppleWin (GitHub)")
    std::vector<std::filesystem::path>  destPaths;        // every file produced (for cleanup)
    std::uint64_t                     expectedBytes = 0;  // 0 = unknown
    bool                              selectable    = true;   // user can toggle the checkbox?
    bool                              selected      = true;   // initial / current checkbox state

    // Performs the entire fetch (HTTP + decode + write). MUST update
    // `bytesDone` as bytes are received and MUST check `cancel`
    // between chunks; on cancel, return E_ABORT and leave any partial
    // file on disk -- the dialog removes it after worker join. On
    // failure, fill `outError` with a short user-facing message.
    std::function<HRESULT (
        std::atomic<std::uint64_t> & bytesDone,
        std::atomic<bool>          & cancel,
        std::string                & outError)>  downloadFn;
};



struct StartupDownloadSet
{
    std::vector<StartupAssetEntry>  entries;

    bool  Empty        () const { return entries.empty(); }
    bool  RequiresRoms () const
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
};



enum class StartupDownloadResult
{
    NothingToDo,
    AllDone,
    PartialDone,
    Skipped,
    Exit
};



class StartupDownloadDialog
{
public:
    static StartupDownloadResult  Show (HINSTANCE                hInstance,
                                        HWND                     hwndOwner,
                                        std::string_view         themeName,
                                        const std::wstring     & machineDisplayName,
                                        StartupDownloadSet     & set);

private:
    enum class  EntryStatus;
    struct      EntryRuntime;
    struct      DialogState;
    struct      RowMetrics;

    static void                WorkerThreadProc      (DialogState * state, size_t index);
    static void                StartWorkers          (DialogState & state);
    static void                JoinAllWorkers        (DialogState & state);
    static void                RemovePartialFiles    (DialogState & state);
    static std::wstring        StatusText            (const EntryRuntime & rt, std::uint64_t expected);
    static void                PaintGroupHeader      (DialogPaintContext  & ctx,
                                                      Label               & hdrLabel,
                                                      const std::wstring  & groupLabel,
                                                      const RowMetrics    & m,
                                                      float                 y);
    static void                PaintEntryRow         (DialogPaintContext       & ctx,
                                                      const StartupAssetEntry  & entry,
                                                      Checkbox                 & cb,
                                                      Label                    & sourceLabel,
                                                      Label                    & statusLabel,
                                                      const std::wstring       & status,
                                                      bool                       downloading,
                                                      bool                       showStatus,
                                                      const RowMetrics         & m,
                                                      float                      y);
    static void                PaintBody             (DialogPaintContext  & ctx,
                                                      StartupDownloadSet  & set,
                                                      DialogState         & state);
    static std::optional<int>  HandleBodyInput       (const DialogInputEvent & ev, DialogState & state);
    static bool                HandleButtonActivated (size_t idx, DialogPrimitive & dlg, DialogState & state);
    static void                HandleTick            (DialogPrimitive & dlg, DialogState & state);
};
