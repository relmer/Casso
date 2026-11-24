#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/IImagePreviews.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer
//
//  The preview of a disk image and of a file in one: the image's catalog, its
//  partitions, its WOZ details, the saved game on a Wizardry or other game
//  disk, and a file read from the image.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImagePreviewer : public IImagePreviews
{
public:
    DiskImagePreviewer (const BrowserParts & parts, const PreviewParts & previewParts);

    void  PreviewImageCatalog (const std::wstring & imagePath) override;
    void  PreviewEntry        (const std::wstring                      & imagePath,
                               const std::string                       & entryPath,
                               const FileEntry                         & entry,
                               VolumeKind                                kind,
                               std::span<const Byte>                     diskSectors = {},
                               std::span<const std::vector<Byte>>        otherDisks  = {}) override;

private:
    using DetailIterator = std::vector<std::pair<std::wstring, std::wstring>>::iterator;

    static constexpr const wchar_t *  kAnalysisHeading = L"Casso analysis";

    bool                      TryDecodeDiskSave (const std::wstring & imagePath, DecodedFile & outFile);
    bool                      PreviewSavedGame  (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind);
    static const FileEntry  * FindSavedGame     (const VolumeListing & listing, VolumeKind kind);
    bool                      PreviewPartitions (const std::wstring & imagePath, PartitionLayoutKind layout, const std::vector<PartitionEntry> & entries);
    void                      AddWozDetails     (const std::wstring & imagePath);

    BrowserParts  m_parts;
    PreviewParts  m_previewParts;
};
