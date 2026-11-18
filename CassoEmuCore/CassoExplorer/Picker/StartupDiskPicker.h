#pragma once

#include "Pch.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/Picker/PickerRequest.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker
//
//  The picker in its startup form: opened before the emulator's window, for
//  the drive the machine being started boots from. It starts on the Casso node's list
//  and has the "Stock disks" and "Demo disks" nodes. A pick boots the
//  machine with the image, "Start without a disk" boots it with the drive
//  empty, and closing the picker quits Casso.
//
//  The stock disks are the DOS 3.3 and ProDOS masters, downloaded when one
//  is picked, and an 800K ProDOS disk built from ProDOS 2.4.3. The demo
//  disks are those in the Apple2\Demos folder that comes with Casso. Each
//  is listed when the drive accepts it. A machine that boots SOS is offered
//  the Apple /// System Utilities disk and no demo disks.
//
////////////////////////////////////////////////////////////////////////////////

class StartupDiskPicker
{
public:
    static constexpr const wchar_t *  kViewKey = L"Startup";

    static PickerRequest              MakeRequest          (const std::wstring & machineName, const ReceivingDrive & drive, const std::wstring & driveLabel,
                                                            std::vector<StockDiskRow> stockDisks, std::vector<StockDiskRow> demoDisks);
    static std::vector<StockDiskRow>  GetStockDisks        (const ReceivingDrive & drive, bool bootsSos = false);
    static std::vector<StockDiskRow>  GetDemoDisks         (IDiskFileIo & fileIo, const ReceivingDrive & drive);
    static bool                       FetchStockDisk       (const std::wstring & path, std::wstring & outError);

    //  The 140K masters offered: the Apple /// System Utilities disk on a
    //  machine that boots SOS, and the DOS 3.3 and ProDOS masters elsewhere.
    static std::vector<AssetBootstrap::StockBootDisk>  GetMasters (bool bootsSos);

    //  Where the 800K stock disk is saved once it is built.
    static std::filesystem::path      GetStockDisk800KPath ();
    static bool                       MakeStockDisk800K    (std::wstring & outError);

    //  `outDiskPath` is the image picked, or empty to start without a disk.
    //  `outIsClosed` is true when the picker was closed, which quits Casso.
    static HRESULT  Run (HINSTANCE              instance,
                         HWND                   owner,
                         const std::wstring   & machineName,
                         const ReceivingDrive & drive,
                         const std::wstring   & driveLabel,
                         const std::string    & activeTheme,
                         bool                   bootsSos,
                         std::wstring         & outDiskPath,
                         bool                 & outIsClosed);
};
