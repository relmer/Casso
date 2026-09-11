#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  User-notification marshaling
//
//  A notification raised off the UI thread is posted rather than shown: the
//  dialog is Dxui, and Dxui asserts UI-thread affinity. lParam carries an
//  owned wstring the message loop deletes.
//
//  The shell the EHM notification sink forwards to is a static member, since
//  the constructor and destructor set and clear it from their own file.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell *  EmulatorShell::s_pNotifyShell = nullptr;

// Reports raised before there is a window to parent a dialog to. File scope
// rather than a shell member because the sink is installed at the top of
// wCassoMain, before the shell is constructed -- command-line and machine-config
// failures happen in that window and must not vanish. Drained once the window
// exists. The CPU thread can append, hence the lock.
static std::vector<std::wstring>  s_pendingNotifications;
static std::mutex                 s_pendingNotifyMutex;





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMachinePicker
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowMachinePicker()
{
    m_machineManager->ShowMachinePicker();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::NotifyUser
//
//  The EHM notification sink. Forwards to the live shell; if there is none
//  (teardown), the message is dropped rather than crashing on a stale
//  pointer -- an error report is not worth taking the process down for.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::NotifyUser (const wchar_t * message)
{
    if (message == nullptr)
    {
        return;
    }

    // No shell yet (startup) or none left (teardown): hold the text rather
    // than drop it. Everything raised this early is replayed the moment there
    // is a window to show it in.
    if (s_pNotifyShell == nullptr)
    {
        QueueNotification (message);
        return;
    }

    s_pNotifyShell->ShowNotification (message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::QueueNotification
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::QueueNotification (const std::wstring & message)
{
    std::lock_guard<std::mutex>  guard (s_pendingNotifyMutex);



    s_pendingNotifications.push_back (message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowNotification
//
//  Shows one notification as a themed modal. Callable from any thread.
//
//  Three cases, in order. With no window yet the text is queued for
//  FlushPendingNotifications -- startup reports a bad prefs file before
//  there is anything to parent a dialog to. Off the UI thread it is posted,
//  because the dialog is Dxui and Dxui asserts UI-thread affinity; a flush
//  failing on the CPU thread takes that path. Otherwise it is shown here.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowNotification (const std::wstring & message)
{
    DialogDefinition  def;
    bool              isOffThread = false;



    if (m_hwnd == nullptr)
    {
        EmulatorShell::QueueNotification (message);
        return;
    }

    isOffThread = (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());

    if (isOffThread)
    {
        wstring *  carried = new (std::nothrow) wstring (message);

        if (carried != nullptr && !PostMessageW (m_hwnd, WM_APP_NOTIFY_USER, 0,
                                                 reinterpret_cast<LPARAM> (carried)))
        {
            delete carried;
        }

        return;
    }

    def.title = L"Casso";
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { message, false, wstring() });
    def.buttons.push_back (DialogButton { L"OK", 0, true, true, false });

    ShowModalDialog (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PostNotification
//
//  Hands the report to the message pump rather than opening a dialog here.
//
//  ShowNotification ends in a modal, which is wrong for a caller that is
//  itself inside a message being handled: the Settings sheet's OK handler
//  reaches one, and a modal opened there runs a nested loop against a sheet
//  that has neither finished committing nor closed. Posting lets the click
//  finish first, so the dialog arrives over a settled window.
//
//  Falls back to the pre-window queue for the same reason ShowNotification
//  does -- a report raised before there is a window must not vanish.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostNotification (const std::wstring & message)
{
    wstring *  carried = nullptr;



    if (m_hwnd == nullptr)
    {
        EmulatorShell::QueueNotification (message);
        return;
    }

    carried = new (std::nothrow) wstring (message);

    if (carried != nullptr && !PostMessageW (m_hwnd, WM_APP_NOTIFY_USER, 0,
                                             reinterpret_cast<LPARAM> (carried)))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FlushPendingNotifications
//
//  Replays anything reported before the window existed. The queue is drained
//  under the lock and shown outside it, because showing is modal and holding
//  a lock across a nested message loop invites a deadlock with a CPU-thread
//  notification arriving mid-dialog.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::FlushPendingNotifications()
{
    std::vector<std::wstring>  pending;
    size_t                     i = 0;



    {
        std::lock_guard<std::mutex>  guard (s_pendingNotifyMutex);

        pending.swap (s_pendingNotifications);
    }

    for (i = 0; i < pending.size(); i++)
    {
        ShowNotification (pending[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowPendingNotificationsWithoutWindow
//
//  The failure-path counterpart of FlushPendingNotifications. Nothing raised
//  before the window exists is shown until CreateEmulatorWindow replays it,
//  and a startup that fails never gets there -- LoadMachineConfig's CHRN /
//  CBRN sites all end in wCassoMain's CHR. A system box is the only surface
//  left, so whatever is still queued is shown through one.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowPendingNotificationsWithoutWindow()
{
    std::vector<std::wstring>  pending;
    size_t                     i = 0;



    {
        std::lock_guard<std::mutex>  guard (s_pendingNotifyMutex);

        pending.swap (s_pendingNotifications);
    }

    for (i = 0; i < pending.size(); i++)
    {
        MessageBoxW (nullptr, pending[i].c_str(), L"Casso", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowModalDialog
//
//  Every modal in Casso goes through the Dxui host path. Kept as its own
//  entry point so callers name the intent rather than the renderer.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowModalDialog (const DialogDefinition & def)
{
    return ShowSimpleDialogViaDxui (def);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowSimpleDialogViaDxui
//
//  Translates a renderable DialogDefinition into a MessageDialog whose
//  content is a DialogBodyContent (wrapped body labels + hyperlink links)
//  plus the action buttons, and shows it modally via ShowModalDialog. The
//  dialog height is derived from the content's preferred (line-count)
//  height so short messages stay compact and long ones grow (clamped).
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowSimpleDialogViaDxui (const DialogDefinition & def)
{
    constexpr int       s_kBaseWidthDip       = 440;
    constexpr int       s_kMaxWidthDip        = 760;
    constexpr int       s_kMinContentWidthDip = 280;   // floor for a self-sizing body
    constexpr int       s_kBodyPadDip         = 56;   // content inset, both sides
    constexpr int       s_kChromeHeightDip    = 108;   // caption + content pad*2 + button row
    constexpr int       s_kMinHeightDip       = 120;
    constexpr int       s_kMaxHeightDip       = 620;
    constexpr int       s_kIconSrcPx          = 256;
    constexpr int       s_kDefaultIconDip     = 48;
    constexpr int       s_kGlyphSizeDip       = 32;
    constexpr wchar_t   s_kchGlyphInfo        = L'\uE946';   // MDL2 Info
    constexpr wchar_t   s_kchGlyphWarning     = L'\uE7BA';   // MDL2 Warning
    constexpr wchar_t   s_kchGlyphError       = L'\uEA39';   // MDL2 ErrorBadge
    constexpr uint32_t  s_kGlyphArgbInfo      = 0xFF4A9EDB;
    constexpr uint32_t  s_kGlyphArgbWarning   = 0xFFF5A623;
    constexpr uint32_t  s_kGlyphArgbError     = 0xFFE5424D;



    std::unique_ptr<DialogBodyContent>  content    = std::make_unique<DialogBodyContent>();
    MessageDialog                       dlg;
    DxuiWindow::CreateParams            params;
    std::vector<MessageDialog::Button>  buttons;
    HRESULT                             hr         = S_OK;
    int                                 heightDip  = 0;
    int                                 widthDip   = 0;
    int                                 result     = -1;
    bool                                imageShown = false;


    content->SetRuns (def.body);

    // A picture takes the app icon's place above the body; a severity glyph
    // sits beside the body and is unaffected.
    if (def.image.has_value())
    {
        imageShown = content->SetImage (*def.image);
    }

    if (!imageShown && (def.icon == DialogIcon::AppPhotoreal || def.icon == DialogIcon::AppFlat))
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW   = 0;
        int                    iconH   = 0;
        int                    iconRes = (def.icon == DialogIcon::AppPhotoreal) ? IDI_CASSO_PHOTOREAL : IDI_CASSO_FLAT_COLOR_HEAD;
        int                    iconDip = (def.iconSizeOverrideDp > 0.0f) ? (int) def.iconSizeOverrideDp : s_kDefaultIconDip;
        HRESULT                hrIcon  = S_OK;


        hrIcon = LoadIconAsPremulBgra (m_hInstance, iconRes, s_kIconSrcPx, iconPixels, iconW, iconH);

        if (SUCCEEDED (hrIcon))
        {
            content->SetIcon (std::move (iconPixels), iconW, iconH, iconDip);
        }
    }
    else if (def.icon == DialogIcon::Info)
    {
        content->SetGlyphIcon (s_kchGlyphInfo, s_kGlyphArgbInfo, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Warning)
    {
        content->SetGlyphIcon (s_kchGlyphWarning, s_kGlyphArgbWarning, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Error)
    {
        content->SetGlyphIcon (s_kchGlyphError, s_kGlyphArgbError, s_kGlyphSizeDip);
    }

    heightDip = std::clamp (s_kChromeHeightDip + content->GetPreferredHeightDip(),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    for (const DialogButton & button : def.buttons)
    {
        buttons.push_back ({ button.label, button.resultCode, button.isDefault, button.isCancel });
    }

    //  WIDE ENOUGH FOR ITS OWN BUTTONS. The width was a hard 440 while the
    //  height already grew with the text, so a dialog whose buttons carried a
    //  filename pushed the row past the left margin and hard against the
    //  frame with no gap at all. No label carries a filename any more, but a
    //  measured row is what keeps that from mattering: measured with the same
    //  estimate the row lays itself out with, so the two cannot disagree.
    widthDip = DxuiButtonRow::kEdgePadDip * 2;

    for (const DialogButton & button : def.buttons)
    {
        widthDip += DxuiButtonRow::GetWidthForLabel (button.label) + DxuiButtonRow::kGapDip;
    }

    widthDip -= DxuiButtonRow::kGapDip;

    // A body of aligned column rows knows how wide it wants to be, and that
    // width varies with what the running machine actually has. Let it set the
    // width -- floor included, so a short body gives a small dialog rather
    // than a standard-width one with a column of air down the right. Prose
    // bodies report 0 (they wrap to whatever they are given) and keep the
    // standard width.
    {
        int  bodyDip = content->GetPreferredWidthDip();

        if (bodyDip > 0)
        {
            bodyDip += s_kBodyPadDip;
            widthDip = (widthDip > bodyDip) ? widthDip : bodyDip;
            widthDip = std::clamp (widthDip, s_kMinContentWidthDip, s_kMaxWidthDip);
        }
        else
        {
            widthDip = std::clamp (widthDip, s_kBaseWidthDip, s_kMaxWidthDip);
        }
    }

    dlg.Configure (std::move (content), std::move (buttons), def.closeBoxResult.value_or (-1));

    params.title                    = def.title;
    params.hInstance                = m_hInstance;
    params.ownerHwnd                = m_hwnd;
    params.initialSizeDip           = { widthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    // Centered on the emulator window rather than on the OS cascade, which
    // ignores the owner: the Help modals opened at the cascade's top-left
    // corner, half off the window that raised them. A modal belongs where
    // the user is already looking.
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (&m_chromeTheme);

    result = dlg.TranslateResult (dlg.ShowModalDialog (dlg.GetDefaultCommandId()));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetPrinterDialogOwner
//
//  Owner HWND for the printer's confirmation / notice boxes. When the preview
//  panel is open the user is acting inside it (its Finish / Copy / Discard
//  buttons, or a menu command while watching it), so own the box by the panel
//  -- the modal box then centers on the panel and disables it while up. With
//  the panel closed the command came from the main menu, so own it by the main
//  window.
//
////////////////////////////////////////////////////////////////////////////////

HWND EmulatorShell::GetPrinterDialogOwner() const
{
    HWND  owner       = m_hwnd;
    bool  panelIsUp   = m_printerPanel != nullptr
                        && m_printerPanel->IsOpen()
                        && m_printerPanel->GetHwnd() != nullptr
                        && IsWindowVisible (m_printerPanel->GetHwnd());



    if (panelIsUp)
    {
        owner = m_printerPanel->GetHwnd();
    }

    return owner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenSettings
//
//  Opens the Settings dialog (View > Settings / Ctrl+,). The bespoke
//  SettingsPanel + SettingsWindow were retired in T162 slice 3d; this shows
//  the DxuiPropertySheet-based SettingsSheet MODELESS (FR-041) so the emulator
//  keeps running behind it. The sheet is heap-owned; its close callback flags
//  a deferred destroy handled by RunMessageLoop. A second invocation while it
//  is already open just re-focuses the existing sheet.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenSettings()
{
    HINSTANCE  hInst = (HINSTANCE) GetWindowLongPtrW (m_hwnd, GWLP_HINSTANCE);



    if (m_settingsSheet != nullptr)
    {
        HWND  existing = m_settingsSheet->GetHwnd();
        if (existing != nullptr)
        {
            SetForegroundWindow (existing);
        }

        return;
    }

    m_settingsSheet = std::make_unique<SettingsSheet>();
    m_settingsSheet->SetOnDialogEnd ([this] (int) { m_settingsSheetClosePending = true; });

    (void) m_settingsSheet->OpenModeless (hInst, m_hwnd,
                                          *m_userConfigStore, m_globalPrefs, *m_themeManager,
                                          *this, m_uiFs);
}
