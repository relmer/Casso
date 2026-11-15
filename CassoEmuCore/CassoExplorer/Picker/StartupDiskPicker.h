#pragma once

#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  StartupDiskPicker
//
//  The picker in its startup form: opened before the emulator's window, for
//  drive 1 of the machine being started. It starts on the Casso node's list
//  and has the "Stock disks" node. A pick boots the machine with the image,
//  "Start without a disk" boots it with drive 1 empty, and closing the
//  picker quits Casso.
//
//  The stock disks are the DOS 3.3 and ProDOS masters, downloaded when one
//  is picked, and the disks bundled with Casso, each listed when the drive
//  accepts it.
//
////////////////////////////////////////////////////////////////////////////////

class StartupDiskPicker
{
public:
    static constexpr const wchar_t *  kViewKey = L"Startup";

    static PickerRequest              MakeRequest    (const std::wstring & machineName, std::vector<StockDiskRow> stockDisks);
    static std::vector<StockDiskRow>  GetStockDisks  (IDiskFileIo & fileIo, const ReceivingDrive & drive);
    static bool                       FetchStockDisk (const std::wstring & path, std::wstring & outError);

    //  `outDiskPath` is the image picked, or empty to start without a disk.
    //  `outIsClosed` is true when the picker was closed, which quits Casso.
    static HRESULT  Run (HINSTANCE           instance,
                         HWND                owner,
                         const std::wstring & machineName,
                         const std::string  & activeTheme,
                         std::wstring       & outDiskPath,
                         bool               & outIsClosed);
};
