#include "Pch.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellDrivePickers.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/EmulatorShell.h"
#include "Shell/Layout/DriveLabel.h"





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

    //  The disks in the machine's drives, for the picker's "In slot 6
    //  drive 1" mark. The store's paths are UTF-8.
    hooks.getMounted = [this] ()
    {
        std::map<std::wstring, std::wstring>  mounted;

        for (const DiskImageStore::MountedSource & source : m_shell.m_machine.GetDiskStore().GetMountedSourcePaths())
        {
            std::u8string  u8 (reinterpret_cast<const char8_t *> (source.path.data()), source.path.size());

            mounted[std::filesystem::path (u8).wstring()] = std::format (L"slot {} drive {}", source.slot, source.drive + 1);
        }

        return mounted;
    };

    return hooks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::Insert
//
//  The mount is queued for the CPU thread, and its completion adds the
//  image to the recent list and its folder to the Casso node's folders, as
//  for any insert.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDrivePickers::Insert (const PickerDrive & drive, const std::wstring & path, std::wstring & outError)
{
    HRESULT  hr = S_OK;



    outError.clear();

    if (drive.address.isRecorder)
    {
        m_shell.m_tapeDeck->InsertTape (path);
        return true;
    }

    hr = m_shell.m_disks->Mount (drive.address.slot, drive.address.drive, path);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDrivePickers::OpenForDiskII
//
////////////////////////////////////////////////////////////////////////////////

void ShellDrivePickers::OpenForDiskII (int drive)
{
    DeskDrive  deskDrive;



    deskDrive.model = m_shell.m_disks->MachineHasBuiltInDrive() ? DeskDriveModel::Disk2c : DeskDriveModel::DiskII;
    deskDrive.slot  = DriveDropRule::kDiskIiSlot;
    deskDrive.drive = drive;

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



    picked.address        = DriveAddress { drive.slot, drive.drive, false };
    picked.receiving.kind = DeskDrivePlan::GetReceivingKind (drive.model);
    picked.label          = DriveLabel::GetTitle (drive, true);

    if (!DriveDropRule::TryGetReceivingDrive (drive.model, picked.receiving))
    {
        return;
    }

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
    HRESULT                              hr    = S_OK;
    std::string                          theme = CassoExplorerPrefs::MapCassoTheme (m_shell.m_settings->GetPrefs().activeTheme);
    std::unique_ptr<ExplorerPickerHost>  host  = std::make_unique<ExplorerPickerHost> (m_shell.m_settings->GetFileSystem(),
                                                                                         m_shell.m_machine.GetBlockMediaFileIo(),
                                                                                         AssetBootstrap::GetAssetBaseDirectory().wstring());



    host->Prepare (std::move (request), theme, m_shell.m_hwnd, cascade);
    host->SetOnClosed (std::move (onClosed));

    hr = host->Open (GetModuleHandle (nullptr));

    if (FAILED (hr))
    {
        return nullptr;
    }

    return std::make_unique<HostDrivePicker> (std::move (host));
}
