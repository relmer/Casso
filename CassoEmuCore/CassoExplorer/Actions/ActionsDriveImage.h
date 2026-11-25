#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/Actions/IAppleDriveLocation.h"
#include "CassoExplorer/Actions/IImageWriteTarget.h"
#include "CassoExplorer/Actions/LinkCompletionGuard.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Machines/Apple2/Common/VolumeImage.h"

class CassoExplorerBrowser;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage
//
//  Whole disks on a linked Apple's drive: read into an image, captured raw,
//  written back, formatted and krunched, and raw sectors and blocks read and
//  written.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsDriveImage
{
public:
    ActionsDriveImage (CassoExplorerBrowser & browser, IImageWriteTarget & target, const IAppleDriveLocation & location);

    ActionsTypes::Outcome  ReadSectors  (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical, const std::wstring & target = {});
    ActionsTypes::Outcome  WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical, const std::wstring & target = {});
    ActionsTypes::Outcome  ReadBlocks   (int block, int count, const std::wstring & hostPath, const std::wstring & target = {});
    ActionsTypes::Outcome  WriteBlocks  (int block, const std::wstring & hostPath, const std::wstring & target = {});

    ActionsTypes::Outcome  ReadDiskToImage  (const std::wstring & imagePath, bool blankUnreadable, ActionsTypes::UnreadableFn onUnreadable);
    ActionsTypes::Outcome  ReadDiskToImage  (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable, bool forceVerify, ActionsTypes::UnreadableFn onUnreadable,
                                             std::function<void ()> onRead = nullptr, std::optional<FullSendProtectionChoices> automatic = std::nullopt);

    //  A raw capture of a linked drive's disk into a WOZ, for a copy-protected
    //  disk; `report` is called with the tracks whose reads differ, if any.
    ActionsTypes::Outcome  CaptureDisk      (const std::string & link, int drive, const std::wstring & imagePath, std::function<void (const std::wstring & report)> report);
    ActionsTypes::Outcome  WriteImageToDisk (const std::wstring & imagePath);

    ActionsTypes::Outcome  WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath, bool forceVerify = false);

    ActionsTypes::Outcome  CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder);

    ActionsTypes::Outcome  FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);
    ActionsTypes::Outcome  FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);

    //  Whether the drive's last catalog read is of an Apple Pascal disk, the
    //  only kind KrunchAppleDrive applies to. outOperation, when given, is the
    //  queued krunch.
    bool                   IsDrivePascal    (const std::string & link, int drive) const;
    ActionsTypes::Outcome  KrunchAppleDrive (const std::string & link, int drive, std::shared_ptr<LinkOperation> * outOperation = nullptr);

    //  The file name a drive's image is given: its ProDOS volume's name with
    //  .po, a DOS 3.3 volume as "DOS 3.3 volume 254.dsk", or the drive.
    static std::wstring  MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive);

    //  MakeDriveImageName for a linked drive, from the catalog last read, or
    //  from the drive's number before a catalog is read.
    static std::wstring  GetDriveImageName (CassoLinkConnections * links, const std::string & link, int slot, int drive);

private:
    CassoExplorerBrowser       & m_browser;
    IImageWriteTarget          & m_target;
    const IAppleDriveLocation  & m_location;
    LinkCompletionGuard          m_guard;
};