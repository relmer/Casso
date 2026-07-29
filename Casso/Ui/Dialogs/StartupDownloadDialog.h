#pragma once

#include "Pch.h"

// The private panel types below derive from these, so the bases must be
// complete here. Everything else this header names stays a forward
// declaration.
#include "Core/DxuiPanel.h"
#include "Window/DxuiDialogWindow.h"

class    DxuiButton;
class    DxuiCheckbox;
class    DxuiLabel;
class    DxuiDpiScaler;
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
//  per-asset progress, and lets the user Exit cleanly (canceling any
//  in-flight work and removing partial files) at any point.
//
//  DxuiButton policy:
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

    // The three panel types Show() assembles. Nested rather than
    // file-scope: a class defined in a .cpp has external linkage, so two
    // translation units defining different types under one name is an ODR
    // violation the linker will not report.

    //
    //  Paint/input bridge that renders the asset rows via the existing
    //  PaintBody callback and forwards mouse events to the per-row
    //  checkboxes via HandleBodyInput. It draws through the concrete
    //  DxuiPainter / DxuiTextRenderer / CassoTheme (the modal host's
    //  actual types) that the legacy DialogPaintContext expects.
    //
    class DownloadBodyPanel : public DxuiPanel
    {
    public:
        void  SetPaintFn (std::function<void (DialogPaintContext &)>     fn) { m_paint = std::move (fn); }
        void  SetInputFn (std::function<void (const DialogInputEvent &)> fn) { m_input = std::move (fn); }

        void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
        void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
        bool  OnMouse (const DxuiMouseEvent & ev) override;

    private:
        std::function<void (DialogPaintContext &)>      m_paint;
        std::function<void (const DialogInputEvent &)>  m_input;
        float                                           m_dpiScale = 1.0f;
    };


    //
    //  Stacks the intro label above the asset-row body, laid out in
    //  physical pixels.
    //
    class DownloadContentPanel : public DxuiPanel
    {
    public:
        void  Init   (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip);
        void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    private:
        DxuiLabel          *  m_intro          = nullptr;
        DownloadBodyPanel  *  m_body           = nullptr;
        int                   m_introHeightDip = 0;
    };


    //
    //  DxuiDialogWindow hosting the intro + asset-row content and the
    //  Download / Skip / Exit buttons. Download carries a custom click
    //  handler (start workers, relabel -- it must NOT close); Skip auto-
    //  closes (leaving the default Skipped result); Exit is the IDCANCEL
    //  button (Escape / close-box) with a custom handler that records the
    //  Exit result. A fast tick polls the workers via the OnPoll hook.
    //
    class DownloadDialog : public DxuiDialogWindow
    {
    public:
        void  ConfigureDownload (std::unique_ptr<DxuiPanel> content, bool requiresRoms, unsigned tickMs);

        void  SetOnDownloadClick (std::function<void()> fn) { m_onDownloadClick = std::move (fn); }
        void  SetOnPoll          (std::function<void()> fn) { m_onPoll          = std::move (fn); }

        DxuiButton *  DownloadButton() const { return m_downloadBtn; }
        DxuiButton *  SkipButton     () const { return m_skipBtn; }
        DxuiButton *  ExitButton     () const { return m_exitBtn; }

    protected:
        void  OnCreate     () override;
        void  OnDialogTick () override;

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

    static void                WorkerThreadProc      (DialogState * state, size_t index);
    static void                StartWorkers          (DialogState & state);
    static void                JoinAllWorkers        (DialogState & state);
    static void                RemovePartialFiles    (DialogState & state);
    static std::wstring        StatusText            (const EntryRuntime & rt, std::uint64_t expected);
    static void                PaintGroupHeader      (DialogPaintContext  & ctx,
                                                      DxuiLabel               & hdrLabel,
                                                      const std::wstring  & groupLabel,
                                                      const RowMetrics    & m,
                                                      float                 y);
    static void                PaintEntryRow         (DialogPaintContext       & ctx,
                                                      const StartupAssetEntry  & entry,
                                                      DxuiCheckbox                 & cb,
                                                      DxuiLabel                    & sourceLabel,
                                                      DxuiLabel                    & statusLabel,
                                                      const std::wstring       & status,
                                                      bool                       downloading,
                                                      bool                       showStatus,
                                                      const RowMetrics         & m,
                                                      float                      y);
    static void                PaintBody             (DialogPaintContext  & ctx,
                                                      StartupDownloadSet  & set,
                                                      DialogState         & state);
    static std::optional<int>  HandleBodyInput       (const DialogInputEvent & ev, DialogState & state);
};
