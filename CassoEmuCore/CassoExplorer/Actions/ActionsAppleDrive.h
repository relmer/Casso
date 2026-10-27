#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/AppleSingleCodec.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsAppleDrive
//
//  Operations on a linked Apple's drives: files put, got, deleted,
//  renamed and made, whole disks read into images and written back, raw
//  sectors and blocks, the link disk, formatting, and the cassette
//  bootstrap.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsAppleDrive
{
public:
    explicit ActionsAppleDrive (CassoExplorerActions & actions);

    bool     TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const;

    ActionsTypes::Outcome  CopyEntriesToAppleDrive (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths);

    //  A file read from an Apple, converted as a copy out of an image is,
    //  and wrapped for HostFileNaming::Style::AppleSingle.
    static HRESULT  ToHostBytes (FilePayload payload, ActionsTypes::Encoding encoding, bool appleSingle, const AppleSingleFile & single, std::vector<Byte> & outBytes);

    //  A host file as the payload a put plan makes of it.
    static HRESULT  MakePayload (const std::wstring & hostPath, const ActionsTypes::PutPlan & plan, const std::vector<Byte> & bytes, VolumeKind kind, FilePayload & outPayload);

    ActionsTypes::Outcome  ReadSectors  (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical);
    ActionsTypes::Outcome  WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical);
    ActionsTypes::Outcome  ReadBlocks   (int block, int count, const std::wstring & hostPath);
    ActionsTypes::Outcome  WriteBlocks  (int block, const std::wstring & hostPath);

    ActionsTypes::Outcome  ReadDiskToImage  (const std::wstring & imagePath, bool blankUnreadable, ActionsTypes::UnreadableFn onUnreadable);
    ActionsTypes::Outcome  ReadDiskToImage  (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable, ActionsTypes::UnreadableFn onUnreadable);
    ActionsTypes::Outcome  WriteImageToDisk (const std::wstring & imagePath);

    ActionsTypes::Outcome  WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath);

    ActionsTypes::Outcome  CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder);

    HRESULT  CheckLinkDisk (const std::string & link, int drive, ActionsTypes::LinkDiskFn then);

    //  The file name a drive's image is given: its ProDOS volume's name with
    //  .po, a DOS 3.3 volume as "DOS 3.3 volume 254.dsk", or the drive.
    static std::wstring  MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive);

    ActionsTypes::Outcome  SaveLinkToDisk   (const std::wstring & tempFolder);

    ActionsTypes::Outcome  FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);
    ActionsTypes::Outcome  FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);

    ActionsTypes::Outcome  SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings);

    ActionsTypes::Outcome  SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand);

    std::wstring  DescribeAppleDrive () const;

    void  CancelLinkOperation ();
    void            GetAppleSingle  (const std::string & image, const FileEntry & entry, const std::wstring & hostPath, ActionsTypes::Outcome & inOutOutcome);

    //  The verbs on an Apple drive. Each queues its operations on the link
    //  and returns at once; each result is reported when the link's
    //  completions run.
    ActionsTypes::Outcome  GetFromAppleDrive        (const std::wstring & hostFolder, HostFileNaming::Style style, const ActionsTypes::CollisionFn & collision);
    ActionsTypes::Outcome  PutOnAppleDrive          (const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress);
    ActionsTypes::Outcome  DeleteOnAppleDrive       ();
    ActionsTypes::Outcome  RenameOnAppleDrive       (const std::string & newName);
    ActionsTypes::Outcome  CreateFolderOnAppleDrive (const std::string & name);
    void     PutItemsOnAppleDrive     (const std::string & link, int drive, const std::string & directory, std::vector<std::string> taken,
                                       const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress, ActionsTypes::Outcome & inOutOutcome);
    void     SubmitWrite              (const std::string & link, int drive, CassoLinkConnections::Queuer queue, ActionsTypes::Outcome & inOutOutcome);

    //  A completion that does nothing once this object is gone. Jobs are in
    //  the shell's connections, which every window shares, and can finish
    //  after the window that queued them, and this object, have closed.
    CassoLinkConnections::Completion  Guard (CassoLinkConnections::Completion inner) const;

private:
    friend class CassoExplorerActions;

    CassoExplorerActions  & m_actions;

    std::shared_ptr<int>    m_alive = std::make_shared<int> (0);
};
