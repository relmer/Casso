#include "Pch.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "CassoExplorer/Picker/StartupDiskPicker.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Machines/Apple2/Common/StockDisk800K.h"
#include "Seams/Win32DiskFileIo.h"
#include "Config/Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::MakeRequest
//
////////////////////////////////////////////////////////////////////////////////

PickerRequest StartupDiskPicker::MakeRequest (const std::wstring & machineName, const ReceivingDrive & drive, const std::wstring & driveLabel,
                                              std::vector<StockDiskRow> stockDisks, std::vector<StockDiskRow> demoDisks)
{
    PickerRequest  request;



    request.form           = PickerForm::ModalOpen;
    request.title          = std::format (L"Choose a boot disk - {}", machineName);
    request.receivingDrive = drive;
    request.driveLabel     = driveLabel;
    request.viewKey        = kViewKey;
    request.filters        = { { L"Disk images", ImageClassifier::GetDiskMask() }, { L"All files", L"*.*" } };
    request.stockDisks     = std::move (stockDisks);
    request.demoDisks      = std::move (demoDisks);
    request.onFetchStock   = &StartupDiskPicker::FetchStockDisk;
    request.confirmLabel   = L"Start";
    request.skipLabel      = L"Start without a disk";

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::GetStockDisks
//
//  The DOS 3.3 and ProDOS masters, which are 140K 5.25-inch disks, and the
//  800K ProDOS disk built from ProDOS 2.4.3. On the Apple ///, which boots
//  SOS, the Apple /// System Utilities disk instead.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<StockDiskRow> StartupDiskPicker::GetStockDisks (const ReceivingDrive & drive, bool bootsSos)
{
    std::vector<StockDiskRow>  rows;
    ImageClass                 master;
    ImageClass                 disk800K;
    std::error_code            ec;



    master.status         = ImageStatus::Classified;
    master.formFactor     = ImageFormFactor::Floppy525;
    master.layout         = ImageLayout::Sector;
    master.formattedBytes = StockDisksListing::kDiskBytes;
    master.blockCount     = StockDisksListing::kDiskBlocks;

    disk800K.status         = ImageStatus::Classified;
    disk800K.formFactor     = ImageFormFactor::Floppy35;
    disk800K.layout         = ImageLayout::Raw;
    disk800K.formattedBytes = StockDisk800K::kBytes;
    disk800K.blockCount     = StockDisk800K::kBlocks;

    if (DriveImageRule::Accepts (drive, master))
    {
        for (AssetBootstrap::StockBootDisk disk : GetMasters (bootsSos))
        {
            std::filesystem::path  path = AssetBootstrap::GetStockBootDiskPath (disk);

            rows.push_back (StockDiskRow { StockBootDisks::GetDisplayName (disk), path.wstring(), AssetBootstrap::IsStockBootDiskCached (disk), StockBootDisks::GetCompatibility (disk), master });
        }
    }

    if (!bootsSos && DriveImageRule::Accepts (drive, disk800K))
    {
        std::filesystem::path  path = GetStockDisk800KPath();

        rows.push_back (StockDiskRow { StockDisk800K::kpszDisplayName, path.wstring(), std::filesystem::exists (path, ec), StockDisk800K::kpszCompatibility, disk800K });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::GetMasters
//
////////////////////////////////////////////////////////////////////////////////

std::vector<AssetBootstrap::StockBootDisk> StartupDiskPicker::GetMasters (bool bootsSos)
{
    if (bootsSos)
    {
        return { AssetBootstrap::StockBootDisk::SosSystemDisk };
    }

    return { AssetBootstrap::StockBootDisk::Dos33Master, AssetBootstrap::StockBootDisk::ProDosUsersDisk, AssetBootstrap::StockBootDisk::ProDos243Disk };
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::GetStockDisk800KPath
//
//  Beside the downloaded stock disks.
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path StartupDiskPicker::GetStockDisk800KPath()
{
    return AssetBootstrap::GetStockBootDiskPath (AssetBootstrap::StockBootDisk::ProDos243Disk).parent_path() / StockDisk800K::kpszFileName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::GetDemoDisks
//
//  Each disk is classified from its file.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<StockDiskRow> StartupDiskPicker::GetDemoDisks (IDiskFileIo & fileIo, const ReceivingDrive & drive)
{
    std::vector<StockDiskRow>    rows;
    std::vector<DiskMru::Entry>  bundled;



    AssetBootstrap::AppendBundledDemoDisks (bundled, [] (const std::wstring & path) { return ImageClassifier::IsImageExtension (std::filesystem::path (path).extension().wstring()); });

    for (const DiskMru::Entry & entry : bundled)
    {
        ImageClass  image = ImageClassifier::Classify (fileIo, TextEncoding::WideToUtf8 (entry.path.wstring()));

        if (DriveImageRule::Accepts (drive, image))
        {
            rows.push_back (StockDiskRow { entry.path.stem().wstring(), entry.path.wstring(), true, std::wstring(), image });
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::FetchStockDisk
//
////////////////////////////////////////////////////////////////////////////////

bool StartupDiskPicker::FetchStockDisk (const std::wstring & path, std::wstring & outError)
{
    HRESULT       hr      = E_INVALIDARG;
    std::wstring  fetched;
    std::string   error;



    outError.clear();

    if (_wcsicmp (GetStockDisk800KPath().c_str(), path.c_str()) == 0)
    {
        return MakeStockDisk800K (outError);
    }

    for (AssetBootstrap::StockBootDisk disk : { AssetBootstrap::StockBootDisk::Dos33Master, AssetBootstrap::StockBootDisk::ProDosUsersDisk, AssetBootstrap::StockBootDisk::ProDos243Disk,
                                                AssetBootstrap::StockBootDisk::SosSystemDisk })
    {
        if (_wcsicmp (AssetBootstrap::GetStockBootDiskPath (disk).c_str(), path.c_str()) == 0)
        {
            hr = AssetBootstrap::EnsureStockBootDisk (disk, fetched, error);
        }
    }

    if (FAILED (hr))
    {
        outError = TextEncoding::Utf8ToWide (error);
    }

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::MakeStockDisk800K
//
//  Downloads ProDOS 2.4.3 when it is not on the disk yet, then builds the
//  800K disk from it.
//
////////////////////////////////////////////////////////////////////////////////

bool StartupDiskPicker::MakeStockDisk800K (std::wstring & outError)
{
    HRESULT          hr      = S_OK;
    Win32DiskFileIo  fileIo;
    std::wstring     fetched;
    std::string      error;
    vector<Byte>     source;
    vector<Byte>     volume;



    outError.clear();

    hr = AssetBootstrap::EnsureStockBootDisk (AssetBootstrap::StockBootDisk::ProDos243Disk, fetched, error);
    CHRF (hr, outError = TextEncoding::Utf8ToWide (error));

    hr = fileIo.ReadAllBytes (TextEncoding::WideToUtf8 (fetched), source);
    CHRF (hr, outError = L"ProDOS 2.4.3 can't be read.");

    hr = StockDisk800K::Build (source, volume);
    CHRF (hr, outError = L"The 800K disk can't be built from ProDOS 2.4.3.");

    hr = fileIo.WriteAllBytes (TextEncoding::WideToUtf8 (GetStockDisk800KPath().wstring()), volume);
    CHRF (hr, outError = L"The 800K disk can't be saved.");

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::Run
//
//  OLE is initialized for the picker's drag and drop and its shell icons,
//  since the emulator's window, which does that, is not made yet.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StartupDiskPicker::Run (HINSTANCE instance, HWND owner, const std::wstring & machineName, const ReceivingDrive & drive, const std::wstring & driveLabel,
                                const std::string & activeTheme, bool bootsSos, std::wstring & outDiskPath, bool & outIsClosed)
{
    HRESULT             hr       = S_OK;
    HRESULT             hrOle    = OleInitialize (nullptr);
    Win32FileSystem     fs;
    Win32DiskFileIo     fileIo;
    ExplorerPickerHost  host (fs, fileIo, AssetBootstrap::GetAssetBaseDirectory().wstring());



    outDiskPath.clear();
    outIsClosed = false;

    // The demo disks are Apple II disks.
    host.Prepare (MakeRequest (machineName, drive, driveLabel, GetStockDisks (drive, bootsSos), bootsSos ? std::vector<StockDiskRow>() : GetDemoDisks (fileIo, drive)),
                  CassoExplorerPrefs::MapCassoTheme (activeTheme), owner, false);

    hr = host.RunModal (instance);
    CHR (hr);

    outIsClosed = !host.GetOutcome().isConfirmed;
    outDiskPath = host.GetOutcome().path;

Error:
    if (SUCCEEDED (hrOle))
    {
        OleUninitialize();
    }

    return hr;
}
