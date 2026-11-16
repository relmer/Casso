#include "Pch.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "CassoExplorer/Picker/StartupDiskPicker.h"
#include "Core/TextEncoding.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellDrivePickers.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/EmulatorShell.h"
#include "Shell/Layout/DriveLabel.h"
#include "Shell/WindowCommandManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostDrivePicker
//
//  A picker window with its host.
//
////////////////////////////////////////////////////////////////////////////////

class HostDrivePicker : public IDrivePicker
{
public:
    explicit HostDrivePicker (std::unique_ptr<ExplorerPickerHost> host) : m_host (std::move (host)) {}

    HWND  GetHwnd() const override      { return m_host->GetHwnd(); }
    void  BringToFront() override       { m_host->BringToFront(); }
    void  Close() override              { m_host->Close(); }

private:
    std::unique_ptr<ExplorerPickerHost>  m_host;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::ShellDrivePickers
//
////////////////////////////////////////////////////////////////////////////////

ShellDrivePickers::ShellDrivePickers (EmulatorShell & shell)
    : m_shell   (shell),
      m_pickers (*this, MakeHooks())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::~ShellDrivePickers
//
////////////////////////////////////////////////////////////////////////////////

ShellDrivePickers::~ShellDrivePickers()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::MakeHooks
//
//  The inspector's question and the door are a Disk II drive's: the
//  inspector edits a 5.25-inch disk, and the other drives have no door
//  that a picker opens.
//
////////////////////////////////////////////////////////////////////////////////

DrivePickers::Hooks ShellDrivePickers::MakeHooks()
{
    DrivePickers::Hooks  hooks;



    hooks.confirmDiscard = [this] (const DriveAddress & address)
    {
        bool  isDiskII = !address.isRecorder && address.slot == DriveDropRule::kDiskIiSlot;

        return !isDiskII || m_shell.ConfirmInspectorDiscard (address.drive, L"Inserting another disk");
    };

    hooks.insert = [this] (const PickerDrive & drive, const std::wstring & path, std::wstring & outError)
    {
        return Insert (drive, path, outError);
    };

    hooks.onOpened = [this] (const DriveAddress & address)
    {
        if (!address.isRecorder && address.slot == DriveDropRule::kDiskIiSlot)
        {
            m_shell.m_disks->OpenDoorForPicker (address.drive);
        }
    };

    hooks.onClosed = [this] (const DriveAddress & address, bool isInserted)
    {
        if (!address.isRecorder && address.slot == DriveDropRule::kDiskIiSlot)
        {
            m_shell.m_disks->RestoreDoorAfterPicker (address.drive, isInserted);
        }
    };

    //  The recorder makes a blank tape through its save request, and the
    //  tape is in the recorder when that returns. A disk drive's picker
    //  creates through the New disk image dialog.
    hooks.getCreateNew = [this] (const PickerDrive & drive) -> std::function<bool (std::wstring & outPath)>
    {
        if (!drive.address.isRecorder)
        {
            return nullptr;
        }

        return [this] (std::wstring & outPath)
        {
            outPath.clear();

            return m_shell.m_tapeDeck->CreateBlankTape();
        };
    };

    //  The DOS 3.3 and ProDOS masters, and apart from them the disks in the
    //  Apple2\Demos folder, each listed for a drive that accepts it. A master
    //  is downloaded when it is picked, as the old dialog's Download rows did.
    hooks.getStockDisks = [] (const PickerDrive & drive)
    {
        return StartupDiskPicker::GetStockDisks (drive.receiving);
    };

    hooks.getDemoDisks = [this] (const PickerDrive & drive)
    {
        return StartupDiskPicker::GetDemoDisks (m_shell.m_machine.GetBlockMediaFileIo(), drive.receiving);
    };

    hooks.fetchStockDisk = [] (const std::wstring & path, std::wstring & outError)
    {
        return StartupDiskPicker::FetchStockDisk (path, outError);
    };

    //  The disks in the machine's drives, for the picker's "In slot 6
    //  drive 1" mark: the Disk II drives', then the 3.5-inch drives' and
    //  hard disks' on the desk. The paths are UTF-8.
    hooks.getMounted = [this] ()
    {
        std::map<std::wstring, std::wstring>  mounted;
        auto                                  add = [&mounted] (const std::string & path, const std::wstring & where)
        {
            std::u8string  u8 (reinterpret_cast<const char8_t *> (path.data()), path.size());

            mounted[std::filesystem::path (u8).wstring()] = where;
        };

        for (const DiskImageStore::MountedSource & source : m_shell.m_machine.GetDiskStore().GetMountedSourcePaths())
        {
            add (source.path, std::format (L"slot {} drive {}", source.slot, source.drive + 1));
        }

        //  An HD20SC is addressed by its SCSI ID.
        if (m_shell.m_scene)
        {
            for (const DeskDrivePlacement & placement : m_shell.m_scene->GetDrivePlan())
            {
                const DeskDrive  & drive = placement.drive;
                std::string        path  = ShellDeskScene::TakesImage (drive) ? m_shell.m_scene->GetDriveImagePath (drive) : std::string();

                if (!path.empty())
                {
                    add (path, (drive.model == DeskDriveModel::HardDisk20SC) ? std::format (L"slot {} ID {}", DriveLabel::GetSlot (drive), drive.drive)
                                                                             : std::format (L"slot {} drive {}", DriveLabel::GetSlot (drive), drive.drive + 1));
                }
            }
        }

        return mounted;
    };

    return hooks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::Insert
//
//  A floppy's mount is queued for the CPU thread, and its completion adds
//  the image to the recent list and its folder to the Casso node's folders,
//  as for any insert. A hard disk's image is staged for the next restart.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDrivePickers::Insert (const PickerDrive & drive, const std::wstring & path, std::wstring & outError)
{
    outError.clear();

    return m_shell.InsertInDrive (drive.address, drive.receiving.kind, drive.label, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::OpenForDiskII
//
//  The drive as the desk labels it: a //c's or //c Plus's drive is a Disk
//  IIc, the //c's drive 1 is built in, and the //c Plus's are on its disk
//  port.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDrivePickers::OpenForDiskII (int drive)
{
    DeskDrive  deskDrive;



    deskDrive.model   = m_shell.m_disks->HasDiskIicDrives() ? DeskDriveModel::Disk2c : DeskDriveModel::DiskII;
    deskDrive.slot    = DriveDropRule::kDiskIiSlot;
    deskDrive.drive   = drive;
    deskDrive.builtIn = m_shell.m_disks->MachineHasBuiltInDrive() && drive == 0;
    deskDrive.onPort  = m_shell.m_disks->HasDiskIicDrives() && !m_shell.m_disks->MachineHasBuiltInDrive();

    OpenForDeskDrive (deskDrive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::OpenForDeskDrive
//
////////////////////////////////////////////////////////////////////////////////

void ShellDrivePickers::OpenForDeskDrive (const DeskDrive & drive)
{
    PickerDrive  picked;



    picked.address = DriveAddress { drive.slot, drive.drive, false };
    picked.label   = DriveLabel::GetTitle (drive, true);

    if (!DriveDropRule::TryGetReceivingDrive (drive.model, m_shell.GetHardDiskBlocks (drive.slot, drive.drive), picked.receiving))
    {
        return;
    }

    m_shell.m_chrome->HideTooltips();

    m_pickers.Open (picked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::OpenForRecorder
//
////////////////////////////////////////////////////////////////////////////////

void ShellDrivePickers::OpenForRecorder()
{
    PickerDrive  picked;



    picked.address.isRecorder = true;
    picked.receiving.kind     = ReceivingDriveKind::Recorder;
    picked.label              = kRecorderLabel;

    m_pickers.Open (picked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::OpenPicker
//
//  The picker is in the emulator's current theme.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IDrivePicker> ShellDrivePickers::OpenPicker (PickerRequest request, bool cascade, std::function<void()> onClosed)
{
    HRESULT                              hr   = S_OK;
    std::unique_ptr<ExplorerPickerHost>  host = MakeHost (std::move (request), m_shell.m_hwnd, cascade);



    host->SetOnClosed (std::move (onClosed));

    hr = host->Open (GetModuleHandle (nullptr));

    if (FAILED (hr))
    {
        return nullptr;
    }

    return std::make_unique<HostDrivePicker> (std::move (host));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::MakeHost
//
//  A picker's host over the emulator's file services, in its theme. A
//  request with no create folder gets the emulator's.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ExplorerPickerHost> ShellDrivePickers::MakeHost (PickerRequest request, HWND owner, bool cascade)
{
    std::string                          theme = CassoExplorerPrefs::MapCassoTheme (m_shell.m_settings->GetPrefs().activeTheme);
    std::unique_ptr<ExplorerPickerHost>  host  = std::make_unique<ExplorerPickerHost> (m_shell.m_settings->GetFileSystem(),
                                                                                         m_shell.m_machine.GetBlockMediaFileIo(),
                                                                                         AssetBootstrap::GetAssetBaseDirectory().wstring());



    if (request.createFolder.empty())
    {
        request.createFolder = m_shell.m_windowCommandManager->GetDiskCreateFolder();
    }

    host->Prepare (std::move (request), theme, owner, cascade);

    return host;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::RunModal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellDrivePickers::RunModal (HWND owner, PickerRequest request, PickerOutcome & outOutcome)
{
    HRESULT                              hr   = S_OK;
    std::unique_ptr<ExplorerPickerHost>  host = MakeHost (std::move (request), (owner != nullptr) ? owner : m_shell.m_hwnd, false);



    m_shell.m_host->BeginModalKeepAlive();
    hr = host->RunModal (GetModuleHandle (nullptr));
    m_shell.m_host->EndModalKeepAlive();

    outOutcome = host->GetOutcome();

    return hr;
}
