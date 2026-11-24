#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

class KrunchMapDialog;
struct CassoLinkBlockDevice;
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager
//
//  A linked Apple's drives: their whole-disk reads, raw captures and
//  writes, writing CassoLink to a disk, formatting and krunching a disk,
//  and the drive a command applies to.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkDriveManager
{
public:
    explicit CassoLinkDriveManager (ExplorerContext & context);
    ~CassoLinkDriveManager();

    //  The whole-disk read and write pickers' history.
    static constexpr GUID  s_kLinkDiskPickerGuid = { 0x71d2ac7c, 0x3b03, 0x4c2c, { 0xac, 0xae, 0x2d, 0xa5, 0x0c, 0x9d, 0x7f, 0x8b } };

    //  Create image's types after Automatic, in the Full Send page's order.
    static constexpr const wchar_t *  s_kImageExtensions[] = { L".dsk", L".woz", L".do", L".po", L".nib", L".nb2", L".d13" };
    static constexpr uint32_t         s_kBlocks400K = 800;    // a 3.5-inch disk's blocks, single- and double-sided
    static constexpr uint32_t         s_kBlocks800K = 1600;

    void  ReadDiskToImage         (const Location & drive = Location(), bool forceVerify = false);
    void  CaptureRawDisk          (const Location & drive, const std::wstring & suggested = std::wstring());
    void  CaptureToImage          (const std::wstring & link, int number, const std::filesystem::path & image);
    void  WriteImageToDisk        (const Location & drive = Location(), bool forceVerify = false);
    void  StartImageWrite         (const Location & drive, const std::wstring & imagePath, bool forceVerify = false);
    bool  TryGetSelectedLinkDrive (Location & outDrive) const;
    bool  TryGetTargetDrive       (const Location & drive, Location & outDrive, std::wstring & outLink, int & outNumber) const;
    bool  IsOnLinkedMachine       () const;
    void  OpenSelectedLinkDrive   ();
    void  SaveLinkToDisk          (const Location & drive = Location());
    void  FormatAppleDisk         (const Location & location);
    void  KrunchAppleDisk         (const Location & location);
    void  SaveLinkDiskImage       (const std::string & name);
    void  SaveLinkProDosDisk      (const std::string & name);
    void  SaveLinkProDosToDisk    (const Location & drive);
    void  SaveLinkSosDisk         (const std::string & name);

private:
    void                 OfferRawCapture   (const Location & drive, const std::filesystem::path & image);
    static std::wstring  GetProDosDiskNote ();

    bool          ChooseProDosDisk   (const std::wstring & title, const std::wstring & note, const std::wstring & confirm, bool destructive, std::filesystem::path & outDisk);
    std::wstring  GetOverwriteTarget (const std::wstring & driveId) const;

    //  A block device's whole-device read and write, in
    //  CassoLinkDriveManagerBlocks.cpp.
    void  ReadBlockDeviceToImage  (const Location & drive, const std::wstring & link, int number, bool forceVerify);
    void  WriteImageToBlockDevice (const Location & drive, const std::wstring & link, int number, bool forceVerify);
    void  StartBlockImageWrite    (const Location & drive, const std::wstring & link, int number, const std::filesystem::path & image, bool forceVerify);
    bool  TryGetBlockDevice       (const std::wstring & link, int number, CassoLinkBlockDevice & outDevice) const;
    bool  AskToResume             (const std::wstring & title, const CassoLinkBlockDevice & device, const std::filesystem::path & image, bool isRead, bool & outResume);
    void  SubmitBlockTransfer     (const std::wstring & link, int number, const std::filesystem::path & image, bool isRead, bool resume, bool forceVerify);

    ExplorerContext                   & m_context;

    //  The last krunch's window, which stays open after the krunch ends.
    std::unique_ptr<KrunchMapDialog>    m_krunchMap;
};
