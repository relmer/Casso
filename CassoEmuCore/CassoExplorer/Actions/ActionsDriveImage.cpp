#include "Pch.h"

#include "CassoExplorer/Actions/ActionsAppleDrive.h"
#include "CassoExplorer/Actions/ActionsCopy.h"
#include "CassoExplorer/Actions/ActionsDriveImage.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/NibbleFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::ActionsDriveImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsDriveImage::ActionsDriveImage (CassoExplorerBrowser & browser, IImageWriteTarget & target, const IAppleDriveLocation & location)
    : m_browser  (browser),
      m_target   (target),
      m_location (location)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::ReadSectors
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::ReadSectors (
    int                          track,
    int                          sector,
    int                          count,
    const std::wstring         & hostPath,
    DiskOperations::Numbering    numbering,
    const std::wstring         & target)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           image   = target.empty() ? m_target.GetFormatTarget() : target;



    CassoExplorerActions::Append (outcome, m_browser.GetOperations().SectorRead (TextEncoding::WideToNarrow (image), numbering,
                                                          track, sector, count, TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::WriteSectors (
    int                          track,
    int                          sector,
    const std::wstring         & hostPath,
    DiskOperations::Numbering    numbering,
    const std::wstring         & image)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           target  = image.empty() ? m_target.GetFormatTarget() : image;



    CassoExplorerActions::Append (outcome, m_browser.GetOperations().SectorWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath),
                                                           numbering, track, sector));

    if (outcome.written > 0)
    {
        m_target.FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::ReadBlocks (int block, int count, const std::wstring & hostPath, const std::wstring & target)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           image   = target.empty() ? m_target.GetFormatTarget() : target;



    CassoExplorerActions::Append (outcome, m_browser.GetOperations().BlockRead (TextEncoding::WideToNarrow (image), block, count,
                                                         TextEncoding::WideToNarrow (hostPath)));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::WriteBlocks (int block, const std::wstring & hostPath, const std::wstring & image)
{
    ActionsTypes::Outcome  outcome;
    std::wstring           target  = image.empty() ? m_target.GetFormatTarget() : image;



    CassoExplorerActions::Append (outcome, m_browser.GetOperations().BlockWrite (TextEncoding::WideToNarrow (target), TextEncoding::WideToNarrow (hostPath), block));

    if (outcome.written > 0)
    {
        m_target.FinishWrite (target);
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::ReadDiskToImage
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::ReadDiskToImage (const std::wstring & imagePath, bool blankUnreadable, ActionsTypes::UnreadableFn onUnreadable)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!m_location.TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return ReadDiskToImage (link, drive, imagePath, blankUnreadable, false, std::move (onUnreadable));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::ReadDiskToImage
//
//  The offer to read again is given the link and drive this read went to.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::ReadDiskToImage (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable, bool forceVerify,
                                                                    ActionsTypes::UnreadableFn onUnreadable, std::function<void()> onRead, std::optional<FullSendProtectionChoices> automatic)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links   = m_browser.GetLinks();
    std::string              path    = TextEncoding::WideToNarrow (imagePath);
    HRESULT                  hr      = S_OK;



    hr = links->Submit (link, [drive, path, blankUnreadable, forceVerify, automatic] (CassoLinkSession & session) { return session.ReadDisk (drive, path, blankUnreadable, forceVerify, automatic); },
                        m_guard.Guard ([onUnreadable, onRead, link, drive] (const LinkOperation & operation) -> std::wstring
    {
        bool  offer = operation.result == HRESULT_FROM_WIN32 (ERROR_CRC) && !operation.unreadable.empty() && !operation.blankUnreadable && onUnreadable;

        if (offer)
        {
            onUnreadable (operation, link, drive);
            return std::wstring();
        }

        if (SUCCEEDED (operation.result) && onRead)
        {
            onRead();
        }

        return FAILED (operation.result) ? CassoLinkConnectionText::DescribeFailure (operation) : std::wstring();
    }));

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::CaptureDisk
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::CaptureDisk (const std::string & link, int drive, const std::wstring & imagePath, std::function<void (const std::wstring & report)> report)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links   = m_browser.GetLinks();
    std::string              path    = TextEncoding::WideToNarrow (imagePath);
    HRESULT                  hr      = S_OK;



    hr = links->Submit (link, [drive, path] (CassoLinkSession & session) { return session.CaptureDisk (drive, path); },
                        m_guard.Guard ([report] (const LinkOperation & operation) -> std::wstring
    {
        if (FAILED (operation.result))
        {
            return CassoLinkConnectionText::DescribeFailure (operation);
        }

        if (report)
        {
            report (TextEncoding::NarrowToWide (operation.message));
        }

        return std::wstring();
    }));

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::WriteImageToDisk
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::WriteImageToDisk (const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!m_location.TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return WriteImageToAppleDrive (link, drive, imagePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::WriteImageToAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath, bool forceVerify)
{
    ActionsTypes::Outcome  outcome;
    std::string            path    = TextEncoding::WideToNarrow (imagePath);



    ActionsAppleDrive::SubmitWrite (*m_browser.GetLinks(), link, drive, [drive, path, forceVerify] (CassoLinkSession & session) { return session.WriteDisk (drive, path, std::string(), forceVerify); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::CreateImageFromAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder)
{
    ActionsTypes::Outcome    outcome;
    CassoLinkConnections   * links     = m_browser.GetLinks();
    std::wstring             name;
    std::wstring             stem;
    std::wstring             extension;
    std::wstring             path;
    std::string              narrow;
    HRESULT                  hr        = S_OK;



    name      = GetDriveImageName (links, link, 6, drive);
    stem      = name.substr (0, name.rfind (L'.'));
    extension = name.substr (name.rfind (L'.'));
    path      = hostFolder + L"\\" + name;

    for (int copy = 2; m_browser.GetOperations().GetFileIo().Exists (TextEncoding::WideToNarrow (path)); copy++)
    {
        path = std::format (L"{}\\{} ({}){}", hostFolder, stem, copy, extension);
    }

    narrow = TextEncoding::WideToNarrow (path);

    hr = links->Submit (link, [drive, narrow] (CassoLinkSession & session) { return session.ReadDisk (drive, narrow, false); },
                        [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnectionText::DescribeFailure (operation) : std::wstring();
    });

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"CassoLink could not be started.");
    }
    else
    {
        outcome.written++;
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::MakeDriveImageName
//
//  Characters a file name cannot have are left out of a volume's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsDriveImage::MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive)
{
    std::wstring  name;



    if (kind == VolumeKind::ProDos && listing.hasVolumeName)
    {
        name = TextEncoding::NarrowToWide (listing.volumeName) + L".po";
    }
    else if (listing.hasVolumeName)
    {
        name = TextEncoding::NarrowToWide (listing.volumeName) + L".dsk";
    }
    else if (listing.hasVolumeNumber)
    {
        name = std::format (L"DOS 3.3 volume {}.dsk", listing.volumeNumber);
    }
    else
    {
        name = std::format (L"Slot {} Drive {}.dsk", slot, drive);
    }

    std::erase_if (name, [] (wchar_t ch) { return wcschr (L"\\/:*?\"<>|", ch) != nullptr; });

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::GetDriveImageName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsDriveImage::GetDriveImageName (CassoLinkConnections * links, const std::string & link, int slot, int drive)
{
    VolumeListing  listing;
    VolumeKind     kind    = VolumeKind::Unknown;
    HRESULT        result  = S_OK;
    std::string    message;



    if (links == nullptr || !links->TryGetListing (link, drive, std::string(), listing, kind, result, message) || FAILED (result))
    {
        listing = VolumeListing();
        kind    = VolumeKind::Unknown;
    }

    return MakeDriveImageName (listing, kind, slot, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::FormatAppleDrive
//
//  The disk is built as a 140 KB image of the format's sector order and
//  formatted there, with the stock system files when it is to be bootable,
//  then written to the drive track by track. A disk with no file system has
//  nothing for the write to check its tracks against, so it is not offered.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    ActionsTypes::Outcome  outcome;
    std::string            link;
    std::string            directory;
    int                    drive     = 0;



    if (!m_location.TryGetAppleDrive (link, drive, directory))
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"Open a drive of a CassoLink connection first.";
        return outcome;
    }

    return FormatAppleDrive (link, drive, request, tempFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::FormatAppleDrive
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder)
{
    ActionsTypes::Outcome           outcome;
    DiskOperations::NewDiskRequest  disk    = request;
    bool                            prodos   = request.formatName == "prodos";
    bool                            isPascal = request.formatName == "pascal";
    bool                            cpm      = request.formatName == "cpm";
    bool                            blocks   = prodos || isPascal;
    std::string                     path;
    std::string                     volume;
    VolumeListing                   listing;
    VolumeKind                      kind     = VolumeKind::Unknown;
    std::vector<Byte>               blank (NibbleFormat::kImageByteSize, 0);
    HRESULT                         hr       = S_OK;



    if (request.formatName != "dos33" && !blocks && !cpm)
    {
        outcome.hr      = E_INVALIDARG;
        outcome.message = L"A disk on the Apple can be formatted as DOS 3.3, ProDOS, Apple Pascal or CP/M.";
        return outcome;
    }

    disk.containerType = blocks ? "po" : "dsk";
    path               = TextEncoding::WideToNarrow (std::format (L"{}\\casso-format-{}.{}", tempFolder, drive, blocks ? L"po" : L"dsk"));

    hr = m_browser.GetOperations().GetFileIo().WriteAllBytes (path, blank);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The new disk could not be made.");
        return outcome;
    }

    CassoExplorerActions::Append (outcome, m_browser.GetOperations().Init (path, disk));

    if (!outcome.Succeeded())
    {
        return outcome;
    }

    outcome.written = 0;

    //  The new volume, as the Apple's label and history have it.
    CassoExplorerActions::Append (outcome, m_browser.GetOperations().List (path, listing, kind));

    volume = isPascal                ? listing.volumeName + ":"
           : cpm                     ? std::string ("CP/M")
           : listing.hasVolumeName   ? "/" + listing.volumeName
           : listing.hasVolumeNumber ? std::format ("VOL {}", listing.volumeNumber)
                                     : std::string (prodos ? "ProDOS" : "DOS 3.3");

    ActionsAppleDrive::SubmitWrite (*m_browser.GetLinks(), link, drive, [drive, path, volume] (CassoLinkSession & session) { return session.WriteDisk (drive, path, volume); }, outcome);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::IsDrivePascal
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsDriveImage::IsDrivePascal (const std::string & link, int drive) const
{
    CassoLinkConnections  * links  = m_browser.GetLinks();
    VolumeListing           listing;
    VolumeKind              kind   = VolumeKind::Unknown;
    HRESULT                 result = S_OK;
    std::string             message;



    if (links == nullptr)
    {
        return false;
    }

    return links->TryGetListing (link, drive, std::string(), listing, kind, result, message) && SUCCEEDED (result) && kind == VolumeKind::Pascal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsDriveImage::KrunchAppleDrive
//
//  The program on the Apple moves the files, so the krunch finishes there
//  when the link drops. The drive's listing is read again when it is done.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsDriveImage::KrunchAppleDrive (const std::string & link, int drive, std::shared_ptr<LinkOperation> * outOperation)
{
    ActionsTypes::Outcome  outcome;



    ActionsAppleDrive::SubmitWrite (*m_browser.GetLinks(), link, drive, [drive, outOperation] (CassoLinkSession & session)
    {
        std::shared_ptr<LinkOperation>  operation = session.Krunch (drive);

        if (outOperation != nullptr)
        {
            *outOperation = operation;
        }

        return operation;
    }, outcome);

    return outcome;
}





