#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "FileTypes/DecodedFile.h"
#include "Machines/Apple2/Common/StockBootDisks.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerImageTools
//
//  The disk image tools: new disks, boot disks, archives, partitions,
//  extraction, inspection, repair and comparison.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerImageTools
{
public:
    explicit ExplorerImageTools (ExplorerContext & context);

    static constexpr uint64_t  kDos33ImageBytes = 35 * 16 * 256;

    void  CreateDisk                (const CassoExplorerNewDiskDialog::Outcome & newDisk);
    bool  CreateDiskForPicker       (std::wstring & outPath);
    bool  TryPrepareBootDisk        (const DiskOperations::NewDiskRequest & request);
    bool  TryDownloadBootDisk       (StockBootDisks::Which disk);
    int   ChooseFormatDefault       (const std::wstring & image);
    void  ChooseFilesystem          (VolumeKind kind);
    void  MakeArchive               (bool shrinkItDisk);
    void  ConvertImage              (ActionsTypes::Verb verb);
    void  CopyPartitionToImage      (const std::wstring & partition);
    void  ReplacePartition          (const std::wstring & partition);
    void  TestImage                 ();
    void  ExtractAll                ();
    void  ExtractImageFiles         (const std::wstring & imagePath);
    void  InspectDiskImages         (const std::vector<std::wstring> & paths);
    void  RepairDiskImage           (const std::wstring & path);
    void  CompareSelectedDiskImages ();
    void  CompareToPick             (const std::wstring & image);

private:
    ExplorerContext  & m_context;
};
