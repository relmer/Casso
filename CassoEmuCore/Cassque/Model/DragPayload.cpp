#include "Pch.h"

#include "Cassque/Model/DragPayload.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"



//  ProDOS's Integer BASIC type, which the volume layer has no constant for.
static constexpr Byte  s_kProDosTypeInteger = 0xFA;





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::CanReceiveFolder
//
////////////////////////////////////////////////////////////////////////////////

bool DragPayload::CanReceiveFolder (VolumeKind kind)
{
    return kind == VolumeKind::ProDos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::GetHostName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DragPayload::GetHostName (
    const FileEntry        & entry,
    VolumeKind               kind,
    HostFileNaming::Style    style,
    bool                   & outConverted)
{
    bool  isDos       = kind == VolumeKind::Dos33;
    bool  isApplesoft = isDos ? (entry.type == Dos33Volume::kTypeApplesoft) : (entry.type == ProDosVolume::kTypeBasic);
    bool  isInteger   = isDos ? (entry.type == Dos33Volume::kTypeInteger)   : (entry.type == s_kProDosTypeInteger);
    bool  isText      = isDos ? (entry.type == Dos33Volume::kTypeText)      : (entry.type == ProDosVolume::kTypeText);



    outConverted = isApplesoft || isInteger || isText;

    if (isApplesoft)
    {
        return HostFileNaming::ForConverted (entry.name, ParsedHostName::ConvertedKind::ApplesoftListing);
    }

    if (isInteger)
    {
        return HostFileNaming::ForConverted (entry.name, ParsedHostName::ConvertedKind::IntegerListing);
    }

    if (isText)
    {
        return HostFileNaming::ForConverted (entry.name, ParsedHostName::ConvertedKind::Text);
    }

    return HostFileNaming::ForRaw (entry.name, kind, entry.type, entry.hasLoadAddress || entry.hasAuxType,
                                   entry.hasLoadAddress ? entry.loadAddress : entry.auxType, style);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::EncodeCatalogEntries
//
////////////////////////////////////////////////////////////////////////////////

std::string DragPayload::EncodeCatalogEntries (
    const std::string               & sourceImage,
    VolumeKind                        kind,
    const std::vector<std::string>  & catalogPaths)
{
    std::string  bytes;



    bytes += sourceImage;
    bytes += '\n';
    bytes += (kind == VolumeKind::Dos33) ? "dos33" : "prodos";
    bytes += '\n';

    for (const std::string & path : catalogPaths)
    {
        bytes += path;
        bytes += '\n';
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::DecodeCatalogEntries
//
////////////////////////////////////////////////////////////////////////////////

bool DragPayload::DecodeCatalogEntries (
    const std::string         & bytes,
    std::string               & outSourceImage,
    VolumeKind                & outKind,
    std::vector<std::string>  & outCatalogPaths)
{
    std::vector<std::string>  lines;
    size_t                    start = 0;
    size_t                    end   = 0;



    outSourceImage.clear();
    outKind = VolumeKind::Unknown;
    outCatalogPaths.clear();

    while ((end = bytes.find ('\n', start)) != std::string::npos)
    {
        lines.push_back (bytes.substr (start, end - start));
        start = end + 1;
    }

    if (lines.size() < 2 || lines[0].empty())
    {
        return false;
    }

    outSourceImage = lines[0];

    if (lines[1] == "dos33")       { outKind = VolumeKind::Dos33; }
    else if (lines[1] == "prodos") { outKind = VolumeKind::ProDos; }
    else                           { return false; }

    outCatalogPaths.assign (lines.begin() + 2, lines.end());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::AppendEntry
//
//  One descriptor per entry; a directory is its own descriptor followed by
//  everything the lister finds beneath it, paths relative to the drop.
//
////////////////////////////////////////////////////////////////////////////////

void DragPayload::AppendEntry (
    const FileEntry          & entry,
    VolumeKind                 kind,
    HostFileNaming::Style      style,
    const std::wstring       & relativeFolder,
    const std::string        & catalogFolder,
    const DirectoryLister    & listDirectory,
    int                        depth,
    std::vector<Descriptor>  & inOutDescriptors)
{
    Descriptor     descriptor;
    bool           substituted = false;
    std::string    catalogPath = catalogFolder.empty() ? entry.name : catalogFolder + "/" + entry.name;
    VolumeListing  listing;
    HRESULT        hr          = S_OK;



    descriptor.catalogPath = catalogPath;
    descriptor.isDirectory = entry.isDirectory;

    if (entry.isDirectory)
    {
        descriptor.relativePath = relativeFolder + HostFileNaming::MakeHostLegal (entry.name, substituted);
    }
    else
    {
        descriptor.relativePath = relativeFolder + GetHostName (entry, kind, style, descriptor.converted);
    }

    inOutDescriptors.push_back (descriptor);

    if (!entry.isDirectory || !listDirectory || depth >= kMaxDirectoryDepth)
    {
        return;
    }

    hr = listDirectory (catalogPath, listing);

    if (FAILED (hr))
    {
        return;
    }

    for (const FileEntry & child : listing.entries)
    {
        AppendEntry (child, kind, style, descriptor.relativePath + L"\\", catalogPath,
                     listDirectory, depth + 1, inOutDescriptors);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload::Build
//
//  Catalog entries offer the private format and the descriptor pair; disk
//  images and host files offer their paths. Nothing offers all three: a
//  receiver that saw both HDROP and descriptors for one drag could take the
//  images as files and the files as images.
//
////////////////////////////////////////////////////////////////////////////////

DragPayload::Plan DragPayload::Build (
    SourceKind                         source,
    const std::string                & sourceImage,
    VolumeKind                         kind,
    const std::vector<FileEntry>     & entries,
    HostFileNaming::Style              style,
    const DirectoryLister            & listDirectory,
    const std::vector<std::wstring>  & hostPaths)
{
    Plan                      plan;
    std::vector<std::string>  catalogPaths;



    switch (source)
    {
        case SourceKind::CatalogEntries:
            for (const FileEntry & entry : entries)
            {
                catalogPaths.push_back (entry.name);

                AppendEntry (entry, kind, style, L"", "", listDirectory, 0, plan.descriptors);
            }

            plan.privateBytes = EncodeCatalogEntries (sourceImage, kind, catalogPaths);
            plan.formats      = { Format::CatalogEntries, Format::FileDescriptors, Format::FileContents };
            break;

        case SourceKind::DiskImages:
        case SourceKind::HostFiles:
        default:
            plan.hostPaths = hostPaths;
            plan.formats   = { Format::HDrop };
            break;
    }

    return plan;
}
