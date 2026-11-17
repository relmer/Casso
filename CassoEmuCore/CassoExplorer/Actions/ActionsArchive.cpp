#include "Pch.h"

#include "CassoExplorer/Actions/ActionsArchive.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Devices/Disk/Containers/ImageContainer.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive::MakeArchive
//
//  An empty archive with the first item's name, then every item put into it.
//  A put that fails returns with the files before it in the archive.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsArchive::MakeArchive (const std::vector<std::wstring> & hostPaths, std::wstring & outPath)
{
    ActionsTypes::Outcome  outcome;
    NuFxArchive            empty;
    std::vector<Byte>      bytes;
    std::filesystem::path  first;
    HRESULT                hr      = S_OK;



    outPath.clear();

    if (hostPaths.empty())
    {
        return outcome;
    }

    first   = hostPaths[0];
    outPath = m_actions.m_copier.MakeFreeFilePath (first.parent_path().wstring(), first.stem().wstring() + L".shk");

    empty.Build (bytes);
    hr = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (outPath), bytes);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, CassoExplorerActions::GetLeafName (outPath) + L" could not be created.");
        outPath.clear();
        return outcome;
    }

    return m_actions.PutInto (outPath, VolumeKind::Archive, std::string(), hostPaths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive::MakeShrinkItDisk
//
//  The image's volume in ProDOS block order, a 140K disk reordered from DOS
//  order, as one disk record with the image's name.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsArchive::MakeShrinkItDisk (const std::wstring & imagePath, std::wstring & outPath)
{
    ActionsTypes::Outcome  outcome;
    std::filesystem::path  path     = imagePath;
    std::vector<Byte>      bytes;
    std::vector<Byte>      sectors;
    std::vector<Byte>      blocks;
    std::vector<Byte>      archived;
    SectorDecodeReport     report;
    NuFxArchive            archive;
    VolumeKind             kind     = VolumeKind::Unknown;
    HRESULT                hr       = S_OK;



    outPath.clear();

    hr = m_actions.m_browser.GetOperations().GetFileIo().ReadAllBytes (TextEncoding::WideToNarrow (imagePath), bytes);

    if (SUCCEEDED (hr))
    {
        hr = VolumeImage::Load (bytes, TextEncoding::WideToNarrow (imagePath), sectors, report);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, path.filename().wstring() + L" doesn't open as a disk image.");
        return outcome;
    }

    kind = VolumeImage::DetectFilesystem (sectors);
    VolumeImage::SaveBlockPayload (sectors, blocks);

    archive.records.push_back (NuFxArchive::MakeDiskRecord (blocks, TextEncoding::WideToNarrow (path.stem().wstring()),
                                                            (kind == VolumeKind::Dos33) ? NuFxRecord::kFsDos33 : NuFxRecord::kFsProDos));
    archive.Build (archived);

    outPath = m_actions.m_copier.MakeFreeFilePath (path.parent_path().wstring(), path.stem().wstring() + L".sdk");
    hr      = m_actions.m_browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (outPath), archived);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, CassoExplorerActions::GetLeafName (outPath) + L" could not be written.");
        outPath.clear();
        return outcome;
    }

    outcome.written = 1;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive::Test
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsArchive::Test (const std::wstring & imagePath, std::wstring & outReport)
{
    ActionsTypes::Outcome   outcome;
    DiskOperations::Result  result = m_actions.m_browser.GetOperations().Test (TextEncoding::WideToNarrow (imagePath));
    std::wstring            leaf   = std::filesystem::path (imagePath).filename().wstring();
    size_t                  at     = 0;



    outcome.hr = result.hr;
    outReport  = TextEncoding::NarrowToWide (result.output + result.message);

    //  The image by its name, not its whole path.
    while ((at = outReport.find (imagePath, at)) != std::wstring::npos)
    {
        outReport.replace (at, imagePath.size(), leaf);
        at += leaf.size();
    }

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsArchive::CanMakeShrinkItDisk
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsArchive::CanMakeShrinkItDisk (const std::wstring & imagePath)
{
    ImageContainerKind  kind = ImageContainer::GetKind (TextEncoding::WideToNarrow (imagePath), 0);



    return kind != ImageContainerKind::Archive && kind != ImageContainerKind::ShrinkItDisk;
}
