#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/MachineManager.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
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

        if (carried != nullptr && !PostMessageW (m_hwnd, (UINT) AppMessage::ShellNotifyUser, 0,
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

    if (carried != nullptr && !PostMessageW (m_hwnd, (UINT) AppMessage::ShellNotifyUser, 0,
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
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::ShowSimpleDialogViaDxui (const DialogDefinition & def)
{
    MessageDialog::AppIconLoader  loadIcon = [this] (DialogIcon icon, int sourcePx, std::vector<uint32_t> & outPixels, int & outWidth, int & outHeight)
    {
        int  resource = (icon == DialogIcon::AppPhotoreal) ? IDI_CASSO_PHOTOREAL : IDI_CASSO_FLAT_COLOR_HEAD;

        return m_window->LoadIconAsPremulBgra (m_hInstance, resource, sourcePx, outPixels, outWidth, outHeight);
    };



    return MessageDialog::ShowDefinition (def, m_hInstance, m_hwnd, &m_chrome->m_chromeTheme, loadIcon);
}
