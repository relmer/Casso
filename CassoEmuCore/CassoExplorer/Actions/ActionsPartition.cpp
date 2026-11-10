#include "Pch.h"

#include "CassoExplorer/Actions/ActionsPartition.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition::TryGetTarget
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsPartition::TryGetTarget (std::wstring & outPartitionPath) const
{
    const CassoExplorerBrowser &  browser  = m_actions.m_browser;
    const std::vector<int>     &  selected = browser.GetSelectedRows();
    Location                      location = browser.GetLocation();
    std::wstring                  parent;
    int                           index    = 0;
    bool                          found    = false;



    outPartitionPath.clear();

    if (browser.IsPartitionListLocation())
    {
        if (selected.size() == 1 && selected[0] >= 0 && selected[0] < (int) browser.GetRows().size())
        {
            outPartitionPath = browser.GetRows()[selected[0]].containerPath;
        }
    }
    else if (browser.IsImageLocation() && location.kind == Location::Kind::DiskImage)
    {
        outPartitionPath = location.path;
    }

    found = TrySplit (outPartitionPath, parent, index);

    if (!found)
    {
        outPartitionPath.clear();
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition::CopyToImage
//
//  The image is written in the folder of the file on the PC, under the card
//  image's name and the partition's number, as a name the folder doesn't
//  have, unless `target` is the path to write.
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsPartition::CopyToImage (const std::wstring & partitionPath, std::wstring & outPath, const std::wstring & target)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io        = m_actions.m_browser.GetOperations().GetFileIo();
    std::filesystem::path  host      = partitionPath.substr (0, partitionPath.find (L'|'));
    std::filesystem::path  step      = partitionPath.substr (partitionPath.rfind (L'|') + 1);
    std::vector<Byte>      bytes;
    std::wstring           parent;
    std::wstring           leaf;
    int                    index     = 0;
    HRESULT                hr        = S_OK;



    outPath.clear();

    if (!TrySplit (partitionPath, parent, index))
    {
        ActionsCopy::AppendMessage (outcome, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), L"No partition is selected.");
        return outcome;
    }

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (partitionPath), bytes);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"Partition {} of {} couldn't be read.", index, CassoExplorerActions::GetLeafName (parent)));
        return outcome;
    }

    leaf    = std::format (L"{} partition {}{}", std::filesystem::path (CassoExplorerActions::GetLeafName (parent)).stem().wstring(), index, step.extension().wstring());
    outPath = target.empty() ? m_actions.m_copier.MakeFreeFilePath (host.parent_path().wstring(), leaf) : target;
    hr      = io.WriteAllBytes (TextEncoding::WideToNarrow (outPath), bytes);

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
//  ActionsPartition::DescribeReplace
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsPartition::DescribeReplace (const std::wstring & partitionPath, const std::wstring & imagePath, std::wstring & outQuestion)
{
    ActionsTypes::Outcome  outcome;
    PartitionEntry         entry;
    std::vector<Byte>      imageBytes;
    std::vector<Byte>      converted;
    std::string            error;
    std::wstring           parent;
    std::wstring           parentText;
    PhysicalDrive          found;
    int                    index     = 0;
    int                    drive     = 0;
    int                    cardIndex = 0;
    HRESULT                hr        = S_OK;



    outQuestion.clear();

    hr = FindEntry (partitionPath, entry);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The partition couldn't be read.");
        return outcome;
    }

    hr = m_actions.m_browser.GetOperations().GetFileIo().ReadAllBytes (TextEncoding::WideToNarrow (imagePath), imageBytes);

    if (SUCCEEDED (hr))
    {
        hr = PartitionAccess::ConvertImage (entry.partition, imageBytes, TextEncoding::WideToNarrow (imagePath), converted, error);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, error.empty() ? CassoExplorerActions::GetLeafName (imagePath) + L" couldn't be read." : TextEncoding::NarrowToWide (error));
        return outcome;
    }

    TrySplit (partitionPath, parent, index);

    //  A card is listed by its drive's display name, as the question also
    //  confirms the write to the card.
    if (PhysicalCards::ParsePath (TextEncoding::WideToUtf8 (parent), drive, cardIndex) == PhysicalCards::PathKind::Card &&
        m_actions.m_browser.GetPhysicalCards() != nullptr && m_actions.m_browser.GetPhysicalCards()->TryGetDrive (drive, found))
    {
        parentText = PhysicalCards::FormatDrive (found);
    }
    else
    {
        parentText = CassoExplorerActions::GetLeafName (parent);
    }

    outQuestion = std::format (L"Replace {} of {} with {} ({})?\r\n\r\nEverything on the partition is overwritten.",
                               TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, true)),
                               parentText,
                               CassoExplorerActions::GetLeafName (imagePath),
                               TextEncoding::NarrowToWide (PartitionAccess::FormatSize (imageBytes.size())));

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition::Replace
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsPartition::Replace (const std::wstring & partitionPath, const std::wstring & imagePath)
{
    ActionsTypes::Outcome  outcome;
    IDiskFileIo          & io      = m_actions.m_browser.GetOperations().GetFileIo();
    PartitionEntry         entry;
    std::vector<Byte>      imageBytes;
    std::vector<Byte>      converted;
    std::string            error;
    HRESULT                hr      = S_OK;



    hr = FindEntry (partitionPath, entry);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, L"The partition couldn't be read.");
        return outcome;
    }

    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (imagePath), imageBytes);

    if (SUCCEEDED (hr))
    {
        hr = PartitionAccess::ConvertImage (entry.partition, imageBytes, TextEncoding::WideToNarrow (imagePath), converted, error);
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, error.empty() ? CassoExplorerActions::GetLeafName (imagePath) + L" couldn't be read." : TextEncoding::NarrowToWide (error));
        return outcome;
    }

    hr = io.WriteAllBytes (TextEncoding::WideToNarrow (partitionPath), converted);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"Partition {} couldn't be written.", entry.partition.index));
        return outcome;
    }

    outcome.written = 1;
    m_actions.FinishWrite (partitionPath);

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition::FindEntry
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ActionsPartition::FindEntry (const std::wstring & partitionPath, PartitionEntry & outEntry)
{
    HRESULT                      hr     = S_OK;
    std::vector<PartitionEntry>  entries;
    std::wstring                 parent;
    PartitionLayoutKind          kind   = PartitionLayoutKind::None;
    int                          index  = 0;
    bool                         split  = TrySplit (partitionPath, parent, index);
    bool                         found  = false;



    CBREx (split, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = m_actions.m_browser.ListPartitions (parent, kind, entries);
    CHR (hr);

    for (const PartitionEntry & entry : entries)
    {
        if (entry.partition.index == index)
        {
            outEntry = entry;
            found    = true;
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition::TrySplit
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsPartition::TrySplit (const std::wstring & partitionPath, std::wstring & outParent, int & outIndex)
{
    size_t  bar   = partitionPath.rfind (L'|');
    bool    split = false;



    outParent.clear();
    outIndex = 0;

    if (bar != std::wstring::npos)
    {
        outParent = partitionPath.substr (0, bar);
        split     = PartitionAccess::TryParseStepName (TextEncoding::WideToNarrow (partitionPath.substr (bar + 1)), outIndex);
    }

    return split;
}
