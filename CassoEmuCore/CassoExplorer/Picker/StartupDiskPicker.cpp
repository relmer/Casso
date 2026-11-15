#include "Pch.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "CassoExplorer/Picker/StartupDiskPicker.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Seams/Win32DiskFileIo.h"
#include "Config/Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker::MakeRequest
//
////////////////////////////////////////////////////////////////////////////////

PickerRequest StartupDiskPicker::MakeRequest (const std::wstring & machineName, std::vector<StockDiskRow> stockDisks, std::vector<StockDiskRow> demoDisks)
{
    PickerRequest  request;



    request.form           = PickerForm::ModalOpen;
    request.title          = std::format (L"Choose a boot disk - {}", machineName);
    request.receivingDrive = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };
    request.driveLabel     = L"Drive 1";
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
//  The DOS 3.3 and ProDOS masters, which are 140K 5.25-inch disks.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<StockDiskRow> StartupDiskPicker::GetStockDisks (const ReceivingDrive & drive)
{
    std::vector<StockDiskRow>  rows;
    ImageClass                 master;



    master.status         = ImageStatus::Classified;
    master.formFactor     = ImageFormFactor::Floppy525;
    master.layout         = ImageLayout::Sector;
    master.formattedBytes = StockDisksListing::kDiskBytes;
    master.blockCount     = StockDisksListing::kDiskBlocks;

    if (DriveImageRule::Accepts (drive, master))
    {
        for (AssetBootstrap::StockBootDisk disk : { AssetBootstrap::StockBootDisk::Dos33Master, AssetBootstrap::StockBootDisk::ProDosUsersDisk })
        {
            std::filesystem::path  path = AssetBootstrap::GetStockBootDiskPath (disk);

            rows.push_back (StockDiskRow { path.stem().wstring(), path.wstring(), AssetBootstrap::IsStockBootDiskCached (disk), master });
        }
    }

    return rows;
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
            rows.push_back (StockDiskRow { entry.path.stem().wstring(), entry.path.wstring(), true, image });
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

    for (AssetBootstrap::StockBootDisk disk : { AssetBootstrap::StockBootDisk::Dos33Master, AssetBootstrap::StockBootDisk::ProDosUsersDisk })
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
//  StartupDiskPicker::Run
//
//  OLE is initialized for the picker's drag and drop and its shell icons,
//  since the emulator's window, which does that, is not made yet.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StartupDiskPicker::Run (HINSTANCE instance, HWND owner, const std::wstring & machineName, const std::string & activeTheme,
                                std::wstring & outDiskPath, bool & outIsClosed)
{
    HRESULT             hr       = S_OK;
    HRESULT             hrOle    = OleInitialize (nullptr);
    Win32FileSystem     fs;
    Win32DiskFileIo     fileIo;
    ExplorerPickerHost  host (fs, fileIo, AssetBootstrap::GetAssetBaseDirectory().wstring());
    ReceivingDrive      drive    { ReceivingDriveKind::Floppy525, 0 };



    outDiskPath.clear();
    outIsClosed = false;

    host.Prepare (MakeRequest (machineName, GetStockDisks (drive), GetDemoDisks (fileIo, drive)), CassoExplorerPrefs::MapCassoTheme (activeTheme), owner, false);

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
