#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/StockDisksListing.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Seams/IHostDialogs.h"
#include "Shell/DiskMru.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerForm
//
//  DriveWindow is a drive's picker: not modal, and a confirm inserts the
//  image through onConfirm. ModalOpen returns an existing file's path to a
//  caller that waits for it. ModalSave returns a path to write, which may
//  not exist.
//
////////////////////////////////////////////////////////////////////////////////

enum class PickerForm
{
    DriveWindow,
    ModalOpen,
    ModalSave,
};





////////////////////////////////////////////////////////////////////////////////
//
//  PickerRequest
//
//  What a Casso Explorer window in picker mode is opened for: the caller's
//  title, starting folder and filter entries, and, for a drive, the drive
//  whose images are listed. The first filter entry is active; a request
//  with none lists every file.
//
////////////////////////////////////////////////////////////////////////////////

struct PickerRequest
{
    PickerForm                     form             = PickerForm::ModalOpen;
    std::wstring                   title;
    std::filesystem::path          initialFolder;
    std::vector<FileDialogFilter>  filters;
    std::optional<ReceivingDrive>  receivingDrive;

    //  "Apple 3.5 Drive, slot 5 drive 1", for the picker's header.
    std::wstring                   driveLabel;

    //  Which saved view the picker opens with: one for each kind of drive,
    //  or the caller's.
    std::wstring                   viewKey;

    //  ModalSave: the name the file name box starts with, the extension a
    //  name typed without one gets (without the dot), and whether the picker
    //  confirms replacing a file.
    std::wstring                   defaultFileName;
    std::wstring                   defaultExtension;
    bool                           askToReplace     = true;

    //  DriveWindow: inserts the image. False leaves the picker open.
    std::function<bool (const std::wstring & path)>  onConfirm;

    //  The caller's create flow, which the recorder has: true with the new
    //  file's path, which is then confirmed, or true with no path when the
    //  flow inserted it. A disk drive without one creates through the New
    //  disk image dialog, in the folder listed or else in `createFolder`.
    std::function<bool (std::wstring & outPath)>     onCreateNew;
    std::filesystem::path                            createFolder;

    //  The "Stock disks" node's rows; with none the tree has no such node.
    //  `onFetchStock` downloads the stock disk at a row's path when the row
    //  is confirmed before its image is on the disk; false when it could not,
    //  which the caller reports.
    std::vector<StockDiskRow>      stockDisks;
    std::function<bool (const std::wstring & path)>  onFetchStock;

    //  The images inserted in the running machine's drives, by path, each
    //  with its drive's label, for the "In drive" mark.
    std::map<std::wstring, std::wstring>  mounted;

    bool  IsSave     () const { return form == PickerForm::ModalSave; }
    bool  IsForDrive () const { return receivingDrive.has_value(); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  PickerOutcome
//
//  What a picker ended with: confirmed with a path, or closed without one.
//
////////////////////////////////////////////////////////////////////////////////

struct PickerOutcome
{
    bool          isConfirmed = false;
    bool          isClosed    = false;
    std::wstring  path;
};
