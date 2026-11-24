#include "Pch.h"

#include "CassoExplorer/Browser/Preview/DiskImagePreviewer.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "FileTypes/DiskSaveTable.h"
#include "FileTypes/WizardryMaze.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::DiskImagePreviewer
//
////////////////////////////////////////////////////////////////////////////////

DiskImagePreviewer::DiskImagePreviewer (const BrowserParts & parts, const PreviewParts & previewParts)
    : m_parts        (parts),
      m_previewParts (previewParts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::PreviewImageCatalog
//
//  The image's catalog, cached; its details when it has no file system.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePreviewer::PreviewImageCatalog (const std::wstring & imagePath)
{
    ImageContents  contents;
    VolumeListing  listing;
    VolumeKind     kind     = VolumeKind::Unknown;
    DecodedFile    diskSave;
    bool           hasSave  = false;



    if (!m_parts.model.TryGetCachedCatalog (imagePath, listing, kind))
    {
        HRESULT  hrDescribe = m_parts.catalog.Describe (imagePath, std::string(), contents);

        IGNORE_RETURN_VALUE (hrDescribe, S_OK);

        listing = std::move (contents.listing);
        kind    = contents.kind;

        //  A disk with no file system may be a game's saved game, such as an
        //  Infocom save disk.
        hasSave = !contents.result.Succeeded() && TryDecodeDiskSave (imagePath, diskSave);

        if (hasSave && m_previewParts.viewState.IsSavedGameShown())
        {
            PreviewDecoder::FromDecoded (diskSave, m_previewParts.preview);
            m_previewParts.preview.hasSavedGame   = true;
            m_previewParts.preview.savedGameShown = true;
            m_previewParts.preview.savedGameName  = diskSave.viewName;
            return;
        }

        if (!contents.result.Succeeded() && PreviewPartitions (imagePath, contents.layout, contents.partitions))
        {
            return;
        }

        if (!contents.result.Succeeded() && PreviewDecoder::ParseDetails (contents.result.message, m_previewParts.preview.details))
        {
            m_previewParts.preview.kind         = PreviewContent::Kind::Details;
            m_previewParts.preview.hasSavedGame  = hasSave;
            m_previewParts.preview.savedGameName = diskSave.viewName;
            AddWozDetails (imagePath);
            return;
        }

        if (!contents.result.Succeeded())
        {
            m_previewParts.preview.kind    = PreviewContent::Kind::Error;
            m_previewParts.preview.message = contents.error;
            return;
        }

        m_parts.model.CacheCatalog (imagePath, listing, kind);
    }

    if (m_previewParts.viewState.IsSavedGameShown() && PreviewSavedGame (imagePath, listing, kind))
    {
        m_previewParts.preview.hasSavedGame   = true;
        m_previewParts.preview.savedGameShown = true;
        return;
    }

    //  A DOS 3.3 or ProDOS disk may also be a game's saved game, such as an
    //  Ultima IV or Ultima V save disk, whose map is in sectors outside its
    //  files.
    hasSave = (kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos) && TryDecodeDiskSave (imagePath, diskSave);

    if (hasSave && m_previewParts.viewState.IsSavedGameShown())
    {
        PreviewDecoder::FromDecoded (diskSave, m_previewParts.preview);
        m_previewParts.preview.hasSavedGame   = true;
        m_previewParts.preview.savedGameShown = true;
        m_previewParts.preview.savedGameName  = diskSave.viewName;
        return;
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_previewParts.preview);
    AddWozDetails (imagePath);

    m_previewParts.preview.hasSavedGame  = hasSave || FindSavedGame (listing, kind) != nullptr;
    m_previewParts.preview.savedGameName = diskSave.viewName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::TryDecodeDiskSave
//
//  The image decoded by DiskSaveTable::DecodeImage, with a companion disk
//  looked for in the same folder. False for an image that can't be read or
//  that no entry accepts.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePreviewer::TryDecodeDiskSave (const std::wstring & imagePath, DecodedFile & outFile)
{
    HRESULT            hr     = S_OK;
    std::string        narrow = TextEncoding::WideToNarrow (imagePath);
    std::vector<Byte>  bytes;



    hr = m_parts.operations.GetFileIo().ReadAllBytes (narrow, bytes);
    CHR (hr);

    hr = DiskSaveTable::DecodeImage (bytes, narrow, [this, &narrow] (DiskSaveTable::IsFoundFn isCompanion, std::vector<Byte> & outSectors)
                                     {
                                         return DiskSaveTable::FindBeside (m_parts.operations.GetFileIo(), narrow, isCompanion, outSectors);
                                     }, outFile);
    CHR (hr);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::FindSavedGame
//
//  A Wizardry scenario disk's SCENARIO.DATA, Wizardry IV's WERDNA.DATA or
//  Wizardry V's PLAYER.DATA, or null for any other disk.
//
////////////////////////////////////////////////////////////////////////////////

const FileEntry * DiskImagePreviewer::FindSavedGame (const VolumeListing & listing, VolumeKind kind)
{
    auto  scenario = std::find_if (listing.entries.begin(), listing.entries.end(),
                                   [] (const FileEntry & entry)
                                   {
                                       return !entry.isDirectory && (_stricmp (entry.name.c_str(), "SCENARIO.DATA") == 0 || _stricmp (entry.name.c_str(), "PLAYER.DATA") == 0
                                                                   || _stricmp (entry.name.c_str(), "WERDNA.DATA") == 0);
                                   });



    return (kind == VolumeKind::Pascal && scenario != listing.entries.end()) ? &*scenario : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::PreviewSavedGame
//
//  A Wizardry disk previews as the saved game in its SCENARIO.DATA or
//  PLAYER.DATA: the roster, and the map of the level the party is on. False,
//  with the preview cleared, for any other disk or a file that doesn't decode.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePreviewer::PreviewSavedGame (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind)
{
    const FileEntry                 * scenario = FindSavedGame (listing, kind);
    std::vector<Byte>                 bytes;
    std::vector<Byte>                 sectors;
    std::vector<std::vector<Byte>>    sides;
    SectorDecodeReport                report;
    std::string                       narrow   = TextEncoding::WideToNarrow (imagePath);



    if (scenario == nullptr)
    {
        return false;
    }

    //  Wizardry V's roster has item numbers, and the text for them is
    //  elsewhere on the disk. Its maze is on the other disks.
    if (_stricmp (scenario->name.c_str(), "PLAYER.DATA") == 0)
    {
        HRESULT  read = m_parts.operations.GetFileIo().ReadAllBytes (narrow, bytes);

        IGNORE_RETURN_VALUE (read, S_OK);
        WizardryMaze::FindSides (m_parts.operations.GetFileIo(), narrow, sides);
    }

    if (!bytes.empty())
    {
        HRESULT  loaded = VolumeImage::Load (bytes, narrow, sectors, report);

        IGNORE_RETURN_VALUE (loaded, S_OK);
    }

    PreviewEntry (imagePath, scenario->name, *scenario, kind, sectors, sides);

    if (m_previewParts.preview.kind != PreviewContent::Kind::Document)
    {
        m_previewParts.preview = PreviewContent();
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::PreviewPartitions
//
//  An image of partitions previews as its partition map and a line for
//  each partition: its label, and its volume and size. False for an image
//  with no partitions.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImagePreviewer::PreviewPartitions (const std::wstring & imagePath, PartitionLayoutKind layout, const std::vector<PartitionEntry> & entries)
{
    if (layout == PartitionLayoutKind::None || entries.empty())
    {
        return false;
    }

    m_previewParts.preview.details.clear();
    m_previewParts.preview.details.emplace_back (L"Partition map", TextEncoding::NarrowToWide (PartitionLayout::GetLayoutName (layout)));

    for (const PartitionEntry & entry : entries)
    {
        m_previewParts.preview.details.emplace_back (DiskImageCatalog::GetPartitionLabel (imagePath, entry, false),
                                        TextEncoding::NarrowToWide (PartitionAccess::GetVolumeLabel (entry) + ", " + PartitionAccess::FormatSize (entry.byteCount)));
    }

    m_previewParts.preview.kind = PreviewContent::Kind::Details;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::AddWozDetails
//
//  A WOZ image's path, for Edit metadata, and for a catalog, the image's WOZ
//  info and metadata as an image with no file system has them in its
//  details, to list above the catalog. Any other image, or a file that can't
//  be read, has neither.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePreviewer::AddWozDetails (const std::wstring & imagePath)
{
    HRESULT                 hr     = S_OK;
    bool                    isWoz  = _wcsicmp (fs::path (imagePath).extension().wstring().c_str(), L".woz") == 0;
    std::vector<Byte>       bytes;
    WozLoader::Description  woz;
    bool                    parsed = false;
    DetailIterator          analysis;



    CBR (isWoz);

    hr = m_parts.operations.GetFileIo().ReadAllBytes (TextEncoding::WideToNarrow (imagePath), bytes);
    CHR (hr);

    WozLoader::Describe (bytes, woz);
    CBR (woz.isWoz);

    m_previewParts.preview.wozPath = imagePath;

    //  The analysis after the WOZ sections is about an image with no catalog,
    //  so a catalog has only the WOZ sections.
    if (m_previewParts.preview.kind == PreviewContent::Kind::Catalog)
    {
        parsed   = PreviewDecoder::ParseDetails (DiskImageSession::DescribeWozChunks (bytes) + DiskImageSession::DescribeContainerHeader (bytes), m_previewParts.preview.details);
        analysis = std::find_if (m_previewParts.preview.details.begin(), m_previewParts.preview.details.end(), [] (const std::pair<std::wstring, std::wstring> & d) { return d.first == kAnalysisHeading && d.second.empty(); });

        m_previewParts.preview.details.erase (analysis, m_previewParts.preview.details.end());
        IGNORE_RETURN_VALUE (parsed, false);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewer::PreviewEntry
//
//  One file in an image, read and rendered: its contents, the details of a
//  file that cannot be read, or why not.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImagePreviewer::PreviewEntry (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind, std::span<const Byte> diskSectors, std::span<const std::vector<Byte>> otherDisks)
{
    DiskOperations::Result  result;
    FilePayload             payload;
    HRESULT                 hr          = S_OK;
    PreviewDecoder::HexForm hexForm     = PreviewDecoder::HexForm::Auto;
    DecodeOptions           options     = m_previewParts.viewState.GetDecodeOptions (imagePath, entryPath);



    result = m_parts.operations.Read (TextEncoding::WideToNarrow (imagePath), entryPath, payload, entry.catalogIndex);

    if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_previewParts.preview.details))
    {
        m_previewParts.preview.kind = PreviewContent::Kind::Details;
        return;
    }

    if (!result.Succeeded())
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = DiskMessageText::FormatImageError (imagePath, result.message);
        return;
    }

    hexForm = m_previewParts.viewState.GetHexForm (imagePath + L"|" + TextEncoding::NarrowToWide (entryPath));
    options.diskSectors = diskSectors;
    options.otherDisks  = otherDisks;
    hr                  = PreviewViewChoices::Render (m_previewParts.viewState.GetViewAs(), entry, kind, payload, hexForm, m_previewParts.bus, m_previewParts.preview, m_previewParts.monitor.GetPreviewMonitor(), m_previewParts.monitor.GetCustomArgb(), options);

    //  A double hi-res picture's monitor follows its form, which is known once
    //  it is decoded.
    if (SUCCEEDED (hr))
    {
        hr = m_previewParts.monitor.RedrawForForm();
    }

    if (FAILED (hr) && m_previewParts.preview.message.empty())
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = L"This file could not be previewed.";
    }
}
