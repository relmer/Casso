#include "Pch.h"
#include "AssetBootstrap.h"
#include "Cassque/Model/KnownFolderStore.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/DiskManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Shell/TapeManager.h"
#include "Core/JsonParser.h"
#include "Core/PathResolver.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Core/TextEncoding.h"
#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/DebugMemoryView.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetDropAction
//
//  A file dropped on a drive mounts there and one dropped on the recorder is
//  inserted into it, but only the kind each takes: a tape dropped on a drive,
//  or a disk on the recorder, is ignored rather than mounted somewhere it
//  cannot be read. A BASIC program loads wherever it is dropped except on
//  the recorder; on the rest of the window, a disk or a tape is ignored, so
//  a drop that misses a drive mounts nothing.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::DropAction EmulatorShell::GetDropAction (int tag, const std::wstring & path)
{
    if (tag == ShellTapeDeck::kTapeDropTag)
    {
        return TapeImageLoader::IsTapeFileExtension (path) ? DropAction::InsertTape : DropAction::None;
    }

    if (tag != kScreenDropTag && IsSupportedDiskImageExtension (path))
    {
        return DropAction::MountDisk;
    }

    return DroppedFiles::IsBasicProgramFile (path) ? DropAction::LoadBasic : DropAction::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnFileDropped
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileDropped (int tag, const std::wstring & path)
{
    switch (GetDropAction (tag, path))
    {
    case DropAction::InsertTape:
        m_tapeDeck->InsertTape (path);
        break;

    case DropAction::MountDisk:
        //  A drop is a hand-off like an insert from another tool: its folder
        //  joins the known-folder list once the mount succeeds, with nobody to
        //  answer.
        m_intentReplies.NoteInsert (tag, fs::path (path).string(), nullptr);
        m_disks->Mount (6, tag, path);
        break;

    case DropAction::LoadBasic:
        LoadDroppedBasic (path);
        break;

    default:
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RegisterScreenDropTarget
//
//  The whole window as a drop target, registered first after the hit
//  tester is cleared so the drives and the recorder, registered after it,
//  are picked ahead of it where they lie.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RegisterScreenDropTarget()
{
    RECT  client = {};



    if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &client) || IsRectEmpty (&client))
    {
        return;
    }

    m_uiShell.GetHitTester().Register (DxuiHitRect { client, DxuiHitSlot::Custom, kScreenDropTag });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LoadDroppedBasic
//
//  UI thread. The file is read and its program tokenized here; the CPU
//  thread writes it into memory (LoadBasicProgram).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadDroppedBasic (const std::wstring & path)
{
    std::ifstream      file  (fs::path (path), std::ios::binary);
    std::vector<Byte>  bytes ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());
    std::vector<Byte>  tokenized;
    std::string        error;
    std::wstring       name  = fs::path (path).filename().wstring();



    if (!file.good() && !file.eof())
    {
        m_chrome->ShowNotice (name + L" could not be read.");
        return;
    }

    if (!DroppedFiles::TryGetBasicProgram (path, bytes, tokenized, error))
    {
        m_chrome->ShowNotice (name + L" was not loaded as a BASIC program: " + TextEncoding::NarrowToWide (error));
        return;
    }

    PostCommand (IDM_BASIC_LOAD, std::string (tokenized.begin(), tokenized.end()));
    m_chrome->ShowNotice (L"Loaded " + name + L" as the BASIC program.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LoadBasicProgram
//
//  CPU thread. Written as BASICLOAD writes it, and noted as a debugger's
//  edit is, so history replays it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadBasicProgram (const std::vector<Byte> & tokenized)
{
    DebugMemoryView  view (m_machine);
    Byte             low  = 0;
    Byte             high = 0;



    view.TryPeek (ApplesoftProgram::kMemSiz,              low);
    view.TryPeek ((Word) (ApplesoftProgram::kMemSiz + 1), high);

    for (const auto & [address, write] : ApplesoftProgram::MakeLoadWrites (tokenized, (Word) (low | (high << 8))))
    {
        for (size_t i = 0; i < write.size(); i++)
        {
            view.TryPoke ((Word) (address + i), write[i]);
        }
    }

    m_machine.NoteDebuggerEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetSecondDriveConnected
//
//  Makes the same live change Settings makes (the command ejects a disk from
//  a drive being removed and lays out the chrome again), then saves it, since
//  no Settings OK will follow to save it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSecondDriveConnected (bool connected)
{
    if (!m_disks->IsSecondDriveOffered() || connected == m_disks->ShouldShowExternalDrive())
    {
        return;
    }

    HandleCommand (connected ? IDM_DRIVE_EXTERNAL_CONNECT : IDM_DRIVE_EXTERNAL_DISCONNECT);
    SaveStorageDevices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetTapeRecorderConnected
//
//  Disconnecting ejects the tape first, which writes back anything recorded
//  onto it, since a disconnected recorder has no tape. The recorder is
//  removed from the drive band, the fullscreen strip and the desk scene
//  alike, which all check IsTapeRecorderShown.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetTapeRecorderConnected (bool connected)
{
    if (!m_tapeDeck->MachineHasCassettePort() || connected == m_tapeDeck->IsRecorderConnected())
    {
        return;
    }

    if (!connected && m_tapeDeck->GetManager() != nullptr)
    {
        m_tapeDeck->GetManager()->Eject();
    }

    m_tapeDeck->SetRecorderConnected (connected);

    m_chrome->ReflowChromeForMachineChange();
    SaveStorageDevices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveStorageDevices
//
//  Writes which storage devices are connected into the machine's saved
//  settings, through the same document Settings writes: the second drive in
//  the machine's own port or its Disk ][ card's, and the recorder in
//  $cassoUiPrefs.
//
//  Read fresh from disk each time, so a Settings sheet open alongside is not
//  overwritten by a stale copy -- and Settings re-reads the drive state on OK
//  for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveStorageDevices()
{
    HRESULT             hr                = S_OK;
    std::string         machineNameNarrow = GetCurrentMachineNameNarrow();
    std::wstring        configRelPath     = std::wstring (L"Machines\\") + m_machine.GetCurrentMachineName() +
                                            L"\\" + m_machine.GetCurrentMachineName() + L".json";
    fs::path            configPath        = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                                PathResolver::GetExecutableDirectory(),
                                                PathResolver::GetWorkingDirectory()),
                                                configRelPath);
    JsonValue           defaultJson;
    JsonValue           mergedJson;
    JsonValue           savedJson;
    JsonParseError      parseErr;
    std::ifstream       configFile;
    std::stringstream   ss;
    SettingsPanelState  state;



    BAIL_OUT_IF (m_settings->GetConfigStore() == nullptr || configPath.empty(), S_OK);

    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), S_OK);

    ss << configFile.rdbuf();

    hr = JsonParser::Parse (ss.str(), defaultJson, parseErr);
    CHRA (hr);

    hr = m_settings->GetConfigStore()->Load (machineNameNarrow, defaultJson, m_settings->GetFileSystem(), mergedJson);
    CHR (hr);

    hr = state.LoadFromMachine (machineNameNarrow, defaultJson, mergedJson);
    CHR (hr);

    if (m_disks->IsSecondDriveOffered())
    {
        state.SetSecondDriveAttached (m_disks->ShouldShowExternalDrive());
    }

    state.SetTapeRecorderConnected (m_tapeDeck->IsRecorderConnected());

    savedJson = state.BuildCurrentJson();

    hr = m_settings->GetConfigStore()->SaveDelta (machineNameNarrow, savedJson, defaultJson, m_settings->GetFileSystem());
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowStorageContextMenu
//
//  Shows a device's own menu on a right-click: the switch that connects it,
//  then the Storage menu's commands for that device. Drive 1 has no such
//  switch; the machine's first drive is always present.
//
//  The rows are the Storage menu's own commands, so their text, enabled
//  state and check marks match the menu's exactly.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowStorageContextMenu (int device, int x, int y)
{
    const EmulatorCommands          & commands = m_chrome->GetMainMenu().GetCommands();
    std::vector<DxuiPopupMenuItem>    items;
    std::vector<int>                  ids;



    if (m_host == nullptr)
    {
        return;
    }

    if (device == kStorageMenuRecorder)
    {
        ids = { IDM_STORAGE_RECORDER, 0, IDM_TAPE_INSERT, IDM_TAPE_PLAY, IDM_TAPE_STOP, IDM_TAPE_REWIND,
                IDM_TAPE_FASTFORWARD, IDM_TAPE_EJECT };
    }
    else if (device == 0)
    {
        // Drive 1 is always there, so its menu is where a detached drive 2 or
        // recorder is attached again: neither is on screen to be clicked.
        bool  drive2Away   = m_disks->IsSecondDriveOffered() && !m_disks->ShouldShowExternalDrive();
        bool  recorderAway = m_tapeDeck->MachineHasCassettePort() && !m_tapeDeck->IsTapeRecorderShown();

        ids = { IDM_DISK_INSERT1, IDM_DISK_EJECT1, IDM_DISK_WP1, IDM_DISK_SALVAGE1 };

        if (drive2Away || recorderAway)
        {
            ids.push_back (0);
        }

        if (drive2Away)
        {
            ids.push_back (IDM_STORAGE_DRIVE2);
        }

        if (recorderAway)
        {
            ids.push_back (IDM_STORAGE_RECORDER);
        }
    }
    else
    {
        ids = { IDM_STORAGE_DRIVE2, 0, IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2 };
    }

    // Every device's menu ends with the auto-hide switch, since the drives and
    // the recorder appear and disappear together.
    ids.push_back (0);
    ids.push_back (IDM_VIEW_AUTOHIDE_DRIVES);

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SendIntentReply
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SendIntentReply (HWND target, const Win32IntentChannel::Reply & reply)
{
    bool  delivered = Win32IntentChannel::SendTo (target, m_hwnd, Win32IntentChannel::GetReplyMessageId(),
                                                  Win32IntentChannel::EncodeReply (reply));



    IGNORE_RETURN_VALUE (delivered, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PostIntentReply
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostIntentReply (HWND target, const Win32IntentChannel::Reply & reply)
{
    IntentReplyPost *  carried = new (std::nothrow) IntentReplyPost();



    if (carried == nullptr)
    {
        return;
    }

    carried->target = target;
    carried->reply  = reply;

    if (m_hwnd == nullptr || !PostMessageW (m_hwnd, (UINT) AppMessage::ShellIntentReply, 0, (LPARAM) carried))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RecordKnownFolder
//
//  Best effort: a hand-off that mounted is not undone because the list could
//  not be written.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordKnownFolder (const std::string & imagePath)
{
    Win32FileSystem   fsKnown;
    KnownFolderStore  store (fsKnown, AssetBootstrap::GetAssetBaseDirectory().wstring());
    std::wstring      folder  = fs::path (imagePath).parent_path().wstring();
    int64_t           nowUnix = (int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                                    std::chrono::system_clock::now().time_since_epoch()).count();
    HRESULT           hr      = S_OK;



    BAIL_OUT_IF (folder.empty(), S_OK);

    hr = store.Append (folder, nowUnix);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InstallIntentReplies
//
//  A reload a tool asked for is decided on the thread that owns disk writes;
//  the answer is composed there and carried to the UI thread to be sent.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InstallIntentReplies()
{
    m_machine.GetDiskStore().SetDecisionSink ([this] (const std::string & path, ChangeAction action,
                                                      bool guestCopyPreserved, const std::string & preservedPath)
    {
        HWND                       replyTo = nullptr;
        Win32IntentChannel::Reply  reply;

        if (!m_intentReplies.TryTakeReload (path, replyTo) || replyTo == nullptr)
        {
            return;
        }

        if (IntentReplyTracker::TryMakeReloadReply (action, guestCopyPreserved, preservedPath, reply))
        {
            PostIntentReply (replyTo, reply);
        }
    });
}
