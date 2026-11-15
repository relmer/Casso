#pragma once

#include "Pch.h"

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
//  StockDiskRow
//
//  A stock system disk the "Stock disks" node lists: its label, where its
//  image is or will be, and whether it has been downloaded.
//
////////////////////////////////////////////////////////////////////////////////

struct StockDiskRow
{
    std::wstring  label;
    std::wstring  path;
    bool          isDownloaded = false;
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

    //  Runs the create flow for the drive and returns the new image's path;
    //  empty hides the command.
    std::function<bool (std::wstring & outPath)>     onCreateNew;

    //  The "Stock disks" node's rows; empty hides the node.
    std::vector<StockDiskRow>      stockDisks;

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
