#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellDrivePickers.h"
#include "Shell/Components/ShellChrome.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/DiskManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Shell/BootDriveRule.h"
#include "Shell/DiskMenuRule.h"
#include "Shell/DriveDropRule.h"
#include "Shell/Layout/DriveLabel.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/SlotCards.h"
#include "Core/TextEncoding.h"
#include "Shell/TapeManager.h"
#include "Core/JsonParser.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnFileDropped
//
//  A file dropped on a drive mounts there and one dropped on the recorder is
//  inserted into it, when DriveImageRule passes the image for that drive.
//  Any other drop does nothing: no mount, no eject and no message, since the
//  cursor over that drive was the no-drop cursor.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileDropped (int tag, const std::wstring & path)
{
    ReceivingDrive  drive;
    DriveAddress    address;
    ImageClass      image    = ImageClassifier::Classify (m_machine.GetBlockMediaFileIo(), TextEncoding::WideToUtf8 (path));
    bool            hasDrive = TryGetDropDrive (tag, drive, address);
    bool            accepted = hasDrive && DriveImageRule::Accepts (drive, image);



    if (accepted)
    {
        InsertInDrive (address, drive.kind, GetDropDriveLabel (tag), path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetDropDriveLabel
//
//  The drive under a drop tag, as the staged-image infobar states it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::GetDropDriveLabel (int tag)
{
    const DriveBarFan  & fan       = m_chrome->m_driveBarFan.GetFan();
    int                  index     = 0;
    int                  barClass  = 0;
    int                  entry     = 0;
    bool                 isExtra   = DriveDropRule::TryGetExtraIndex (tag, index);
    bool                 isBar     = DriveDropRule::TryGetBarEntry (tag, barClass, entry);



    if (isExtra && index < m_scene->m_deskScene.GetExtraCount())
    {
        return DriveLabel::GetTitle (m_scene->m_deskScene.GetExtraDrive (index), true);
    }

    if (isBar && barClass < fan.GetClassCount() && entry < fan.GetEntryCount (barClass))
    {
        return DriveLabel::GetTitle (fan.GetDrive (barClass, entry), true);
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InsertInDrive
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::InsertInDrive (const DriveAddress & address, ReceivingDriveKind kind, const std::wstring & label, const std::wstring & path)
{
    HRESULT  hr = S_OK;



    if (address.isRecorder)
    {
        m_tapeDeck->InsertTape (path);
        return true;
    }

    //  A hard disk's image takes a restart, which is confirmed first.
    if (kind == ReceivingDriveKind::HardDisk)
    {
        ChangeHardDiskImage (address.slot, address.drive, label, path);
        return true;
    }

    hr = m_disks->Mount (address.slot, address.drive, path);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RunModalPicker
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::RunModalPicker (HWND owner, PickerRequest request, PickerOutcome & outOutcome)
{
    return m_drivePickers->RunModal (owner, std::move (request), outOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetHardDiskBlocks
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::GetHardDiskBlocks (int slot, int port) const
{
    for (const SlotConfig & config : m_machine.GetConfig().slots)
    {
        if (config.slot == slot && config.enabled && port >= 0 && (size_t) port < config.ports.size())
        {
            const BlockDriveSpec *  drive = BlockDeviceCatalog::FindDrive (config.ports[(size_t) port].device);

            return (drive != nullptr) ? drive->maxBlocks : 0;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TryGetDropDrive
//
//  The drive under a drop tag: a Disk II drive, the recorder, or a drive on
//  the desk or the drive bar that takes a disk while the machine runs.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryGetDropDrive (int tag, ReceivingDrive & outDrive, DriveAddress & outAddress)
{
    const DriveBarFan  & fan       = m_chrome->m_driveBarFan.GetFan();
    int                  index     = 0;
    int                  barClass  = 0;
    int                  entry     = 0;
    bool                 isExtra   = DriveDropRule::TryGetExtraIndex (tag, index);
    bool                 isBar     = DriveDropRule::TryGetBarEntry (tag, barClass, entry);
    const DeskDrive    * deskDrive = nullptr;



    outDrive   = ReceivingDrive();
    outAddress = DriveAddress();

    if (DriveDropRule::IsRecorderTag (tag))
    {
        outDrive.kind         = ReceivingDriveKind::Recorder;
        outAddress.isRecorder = true;
        return true;
    }

    if (DriveDropRule::IsDiskIiTag (tag))
    {
        outDrive.kind    = ReceivingDriveKind::Floppy525;
        outAddress.slot  = DriveDropRule::kDiskIiSlot;
        outAddress.drive = tag;
        return true;
    }

    if (isExtra && index < m_scene->m_deskScene.GetExtraCount())
    {
        deskDrive = &m_scene->m_deskScene.GetExtraDrive (index);
    }
    else if (isBar && barClass < fan.GetClassCount() && entry < fan.GetEntryCount (barClass))
    {
        deskDrive = &fan.GetDrive (barClass, entry);
    }

    if (deskDrive == nullptr)
    {
        return false;
    }

    outAddress.slot  = deskDrive->slot;
    outAddress.drive = deskDrive->drive;

    return DriveDropRule::TryGetReceivingDrive (deskDrive->model, GetHardDiskBlocks (deskDrive->slot, deskDrive->drive), outDrive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsDropAccepted
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsDropAccepted (int tag, const ImageClass & image)
{
    ReceivingDrive  drive;
    DriveAddress    address;
    bool            hasDrive = TryGetDropDrive (tag, drive, address);



    return hasDrive && DriveImageRule::Accepts (drive, image);
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

    if (!connected)
    {
        m_drivePickers->Close (DriveAddress { 0, 0, true });
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
//  settings: the second drive in the machine's port or its Disk ][ card's,
//  and the recorder in $cassoUiPrefs.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveStorageDevices()
{
    SaveMachineSettings ([this] (SettingsPanelState & state)
    {
        if (m_disks->IsSecondDriveOffered())
        {
            state.SetSecondDriveAttached (m_disks->ShouldShowExternalDrive());
        }

        state.SetTapeRecorderConnected (m_tapeDeck->IsRecorderConnected());
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveHardDiskImage
//
//  Writes a hard disk's new image into the machine's saved settings, where
//  the Machine tab has it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveHardDiskImage (int slot, int drive, const std::string & path)
{
    SaveMachineSettings ([slot, drive, &path] (SettingsPanelState & state)
    {
        HRESULT  hr = state.SetSlotPortImage (slot, (size_t) drive, path);



        IGNORE_RETURN_VALUE (hr, S_OK);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MakeHardDiskChangeDialog
//
////////////////////////////////////////////////////////////////////////////////

DialogDefinition EmulatorShell::MakeHardDiskChangeDialog (const std::wstring & driveTitle, const std::wstring & fileName)
{
    DialogDefinition  def;



    def.title = L"Change hard disk image";
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { std::format (L"Changing the image in {} to {} restarts the machine now.", driveTitle, fileName), false, std::wstring() });
    def.body.push_back (DialogTextRun { L"Anything the running program hasn't saved is lost.", false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Restart", kHardDiskChangeRestart, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0,                      false, true,  false });

    return def;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangeHardDiskImage
//
//  UI thread. On Restart the image is attached on the CPU thread, which
//  restarts the machine once it is attached; on Cancel nothing changes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangeHardDiskImage (const DeskDrive & drive, const std::wstring & path)
{
    ChangeHardDiskImage (drive.slot, drive.drive, ShellDeskScene::GetDriveTitle (drive), path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangeHardDiskImage
//
//  As above, for a drive given by its address and title, as a drive's picker
//  has it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangeHardDiskImage (int slot, int drive, const std::wstring & driveTitle, const std::wstring & path)
{
    int  choice = 0;



    m_chrome->HideTooltips();

    choice = ShowModalDialog (MakeHardDiskChangeDialog (driveTitle, fs::path (path).filename().wstring()));

    if (choice == kHardDiskChangeRestart)
    {
        PostCommand (IDM_HARDDISK_ATTACH, std::format ("{} {} {}", slot, drive, TextEncoding::WideToUtf8 (path)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BrowseHardDisk
//
//  The drive's picker, whose pick is confirmed and attached as a drop is.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseHardDisk (const DeskDrive & drive)
{
    m_drivePickers->OpenForDeskDrive (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveMachineSettings
//
//  Applies `edit` to the machine's saved settings, through the same document
//  Settings writes.
//
//  Read fresh from disk each time, so a Settings sheet open alongside is not
//  overwritten by a stale copy -- and Settings re-reads the drive state on OK
//  for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveMachineSettings (const std::function<void (SettingsPanelState &)> & edit)
{
    HRESULT  hr = EditSavedMachine ([&edit] (SettingsPanelState & state)
    {
        edit (state);
        return S_OK;
    });



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EditSavedMachine
//
//  Reads the running machine's saved settings, applies `edit`, and writes
//  them back, through the same document Settings writes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::EditSavedMachine (const std::function<HRESULT (SettingsPanelState & state)> & edit)
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
    JsonParseError      parseErr;
    std::ifstream       configFile;
    std::stringstream   ss;



    BAIL_OUT_IF (m_settings->GetConfigStore() == nullptr || configPath.empty(), HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    ss << configFile.rdbuf();

    hr = JsonParser::Parse (ss.str(), defaultJson, parseErr);
    CHRA (hr);

    hr = SettingsPanelState::EditSaved (*m_settings->GetConfigStore(), m_settings->GetFileSystem(), machineNameNarrow, defaultJson, edit);
    CHR (hr);

Error:
    return hr;
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
    const EmulatorCommands          & commands = m_chrome->m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;
    std::vector<int>                  ids;



    if (m_host == nullptr)
    {
        return;
    }

    if (device >= kStorageMenuBar)
    {
        const DriveBarFan  & fan   = m_chrome->m_driveBarFan.GetFan();
        int                  c     = (device - kStorageMenuBar) / ShellDeskScene::kBarDropClassStride;
        int                  entry = (device - kStorageMenuBar) % ShellDeskScene::kBarDropClassStride;



        if (entry < fan.GetEntryCount (c))
        {
            ShowDriveContextMenu (fan.GetDrive (c, entry), x, y);
        }

        return;
    }

    if (device >= kStorageMenuDeskExtra)
    {
        if (device - kStorageMenuDeskExtra < m_scene->m_deskScene.GetExtraCount())
        {
            ShowDriveContextMenu (m_scene->m_deskScene.GetExtraDrive (device - kStorageMenuDeskExtra), x, y);
        }

        return;
    }

    // A //c Plus's drive 2 commands act on its first 5.25-inch drive, and its
    // second has only insert and eject.
    if (device != kStorageMenuRecorder && DiskMenuRule::HasBuiltIn35Drive (m_machine.GetConfig()))
    {
        if (device != 0)
        {
            ShowDriveContextMenu (DeskDrive { DeskDriveModel::Disk2c, BootDriveRule::kDiskIiSlot, device }, x, y);
            return;
        }

        ids = { IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2, IDM_DISK_INSPECT2, IDM_DISK_THUMBNAIL2 };
    }
    else if (device == kStorageMenuRecorder)
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

        ids = { IDM_DISK_INSERT1, IDM_DISK_EJECT1, IDM_DISK_WP1, IDM_DISK_SALVAGE1, IDM_DISK_INSPECT1, IDM_DISK_THUMBNAIL1 };

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
        ids = { IDM_STORAGE_DRIVE2, 0, IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2, IDM_DISK_INSPECT2, IDM_DISK_THUMBNAIL2 };
    }

    // Every device's menu ends with the auto-hide switch, since the drives and
    // the recorder appear and disappear together.
    ids.push_back (0);
    ids.push_back (IDM_VIEW_AUTOHIDE_DRIVES);
    ids.push_back (IDM_VIEW_FLOPPIES_ON_DESK);
    ids.push_back (IDM_VIEW_HARD_DISKS_ON_DESK);
    ids.push_back (IDM_VIEW_FLOPPY_PAIR_BESIDE);

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowDriveContextMenu
//
//  A floppy drive's or hard disk's menu: the drive's name, then for a
//  floppy drive its insert and eject, and for a hard disk its image
//  change, then the switches every storage menu ends with.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowDriveContextMenu (const DeskDrive & drive, int x, int y)
{
    const EmulatorCommands          & commands = m_chrome->m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;



    if (m_host == nullptr)
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForHeader (ShellDeskScene::GetDriveTitle (drive)));

    if (drive.model == DeskDriveModel::Disk2c)
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label     = L"&Insert disk...";
        insert->dispatch  = [this, drive] () { m_disks->BrowseForDisk (drive.drive); };
        eject->label      = L"&Eject disk";
        eject->dispatch   = [this, drive] () { m_disks->Eject (drive.slot, drive.drive); };
        eject->isEnabled  = [this, drive] () { return !m_disks->GetMountedImagePath (drive.drive).empty(); };

        items.push_back (DxuiPopupMenuItem::ForCommand (insert));
        items.push_back (DxuiPopupMenuItem::ForCommand (eject));
    }
    else if (ShellDeskScene::IsFloppy35 (drive))
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label     = L"&Insert disk...";
        insert->dispatch  = [this, drive] () { m_scene->BrowseDeskDrive (drive); };
        eject->label      = L"&Eject disk";
        eject->dispatch   = [this, drive] () { m_disks->Eject (drive.slot, drive.drive); };
        eject->isEnabled  = [this, drive] () { return !m_scene->GetDriveImagePath (drive).empty(); };

        items.push_back (DxuiPopupMenuItem::ForCommand (insert));
        items.push_back (DxuiPopupMenuItem::ForCommand (eject));
    }
    else if (ShellDeskScene::IsHardDisk (drive))
    {
        auto  change = std::make_shared<DxuiCommand>();



        change->label    = L"&Change image...";
        change->dispatch = [this, drive] () { BrowseHardDisk (drive); };

        items.push_back (DxuiPopupMenuItem::ForCommand (change));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int id : { IDM_VIEW_AUTOHIDE_DRIVES, IDM_VIEW_FLOPPIES_ON_DESK, IDM_VIEW_HARD_DISKS_ON_DESK, IDM_VIEW_FLOPPY_PAIR_BESIDE })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}
