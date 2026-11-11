#include "Pch.h"

#include "CassoExplorer/Model/CollectionWalk.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskIdentity.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::Walk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CollectionWalk::Walk (IDiskFileIo & io, const std::string & root, const Visitor & visitor)
{
    HRESULT                   hr = S_OK;
    std::vector<std::string>  rootFiles;



    //  A root that can't be listed is an error; a subfolder that can't be is
    //  passed over.
    hr = io.ListFiles (root, rootFiles);
    CHR (hr);

    WalkFolder (io, root, visitor);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkFolder
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkFolder (IDiskFileIo & io, const std::string & folder, const Visitor & visitor)
{
    std::vector<std::string>  files;
    std::vector<std::string>  folders;
    HRESULT                   hr = S_OK;



    hr = io.ListFiles (folder, files);
    IGNORE_RETURN_VALUE (hr, S_OK);

    std::sort (files.begin(), files.end());

    for (const std::string & name : files)
    {
        std::string        path   = folder + "\\" + name;
        std::vector<Byte>  bytes;
        FileStamp          stamp;
        HRESULT            hrStat = S_OK;
        HRESULT            hrRead = S_OK;

        if (IsStopped (visitor))
        {
            return;
        }

        if (!IsContainerName (name))
        {
            continue;
        }

        hrStat = io.Stat (path, stamp);

        if (FAILED (hrStat) || stamp.sizeBytes > kLargestImage)
        {
            continue;
        }

        hrRead = io.ReadAllBytes (path, bytes);

        if (SUCCEEDED (hrRead))
        {
            WalkBytes (path, name, std::move (bytes), 0, visitor);
        }
    }

    hr = io.ListFolders (folder, folders);
    IGNORE_RETURN_VALUE (hr, S_OK);

    std::sort (folders.begin(), folders.end());

    for (const std::string & name : folders)
    {
        WalkFolder (io, folder + "\\" + name, visitor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkBytes
//
//  A container's bytes: a ZIP's members, a gzip's contents, or the image or
//  archive the bytes load as, with any partitions on it. An image is listed
//  as a disk unless its partitions fill it, as a CFFA card's or a
//  MicroDrive's do, or it has partitions and no file system Casso reads.
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkBytes (const std::string & path, const std::string & name, std::vector<Byte> && bytes, int depth, const Visitor & visitor)
{
    std::vector<Byte>            sectors;
    std::vector<Byte>            expanded;
    std::string                  innerName;
    std::string                  error;
    SectorDecodeReport           report;
    VolumeKind                   kind     = VolumeKind::Unknown;
    std::unique_ptr<IVolume>     volume;
    std::vector<PartitionEntry>  parts;
    PartitionLayoutKind          layout   = PartitionLayoutKind::None;
    bool                         isLoaded = false;
    bool                         isFilled = false;
    HRESULT                      hr       = S_OK;



    if (depth > kMaxDepth || IsStopped (visitor))
    {
        return;
    }

    if (HasExtension (name, ".zip"))
    {
        WalkZip (path, bytes, depth, visitor);
        return;
    }

    //  A gzip's contents have the gzip's path, as a ContainerPath reads it.
    if (HasExtension (name, ".gz") && GzipFile::IsGzip (bytes))
    {
        hr = GzipFile::Expand (bytes, expanded, innerName, error);

        if (SUCCEEDED (hr))
        {
            WalkBytes (path, name.substr (0, name.size() - 3), std::move (expanded), depth, visitor);
        }

        return;
    }

    hr = PartitionAccess::List (bytes, name, layout, parts);

    if (FAILED (hr))
    {
        parts.clear();
    }

    hr       = VolumeImage::Load (bytes, name, sectors, report);
    isLoaded = SUCCEEDED (hr);
    kind     = isLoaded ? VolumeImage::DetectFilesystem (sectors) : VolumeKind::Unknown;

    if (kind == VolumeKind::Archive)
    {
        volume = VolumeImage::CreateVolume (kind, sectors);

        if (volume != nullptr)
        {
            WalkArchive (path, *volume, depth, visitor);
        }

        return;
    }

    isFilled = layout == PartitionLayoutKind::Cffa || layout == PartitionLayoutKind::FocusDrive
            || layout == PartitionLayoutKind::MicroDrive || layout == PartitionLayoutKind::Ppm;

    if (isLoaded && !isFilled && (kind != VolumeKind::Unknown || parts.empty()))
    {
        WalkDisk (path, name, sectors, report, kind, depth, visitor);
    }

    WalkParts (path, name, bytes, parts, depth, visitor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkZip
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkZip (const std::string & path, std::span<const Byte> bytes, int depth, const Visitor & visitor)
{
    std::vector<ZipEntry>  entries;
    HRESULT                hr = S_OK;



    hr = ZipArchive::Extract (bytes, entries);

    if (FAILED (hr))
    {
        return;
    }

    for (ZipEntry & entry : entries)
    {
        if (!entry.isDirectory && IsContainerName (entry.path))
        {
            WalkBytes (path + ContainerPath::kPathSeparator + entry.path, GetLeaf (entry.path), std::move (entry.data), depth + 1, visitor);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkArchive
//
//  The entries of a NuFX or Binary II archive. A disk in an archive has no
//  extension and is in ProDOS order, so it loads as a `.po`.
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkArchive (const std::string & path, const IVolume & volume, int depth, const Visitor & visitor)
{
    VolumeListing  listing;
    HRESULT        hr = S_OK;



    hr = volume.Enumerate (listing);

    if (FAILED (hr))
    {
        return;
    }

    for (const FileEntry & entry : listing.entries)
    {
        FilePayload  payload;
        bool         isDisk  = false;
        std::string  name    = GetLeaf (entry.name);

        if (entry.isDirectory || IsStopped (visitor))
        {
            continue;
        }

        hr = volume.Read (FilePath::Parse (entry.name), payload);

        if (FAILED (hr))
        {
            continue;
        }

        isDisk = name.find ('.') == std::string::npos && payload.bytes.size() >= s_kFloppyBytes && payload.bytes.size() % s_kBlockBytes == 0;

        if (isDisk || IsContainerName (name) || entry.type == s_kArchiveType)
        {
            WalkBytes (path + ContainerPath::kPathSeparator + entry.name, isDisk ? name + ".po" : name, std::move (payload.bytes), depth + 1, visitor);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkParts
//
//  The partitions found on an image, each walked as a separate image.
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkParts (const std::string & path, const std::string & name, const std::vector<Byte> & bytes, const std::vector<PartitionEntry> & parts, int depth, const Visitor & visitor)
{
    for (const PartitionEntry & entry : parts)
    {
        std::vector<Byte>  part;
        std::string        error;
        int                index = 0;
        HRESULT            hr    = S_OK;

        if (IsStopped (visitor) || !PartitionAccess::TryParseStepName (entry.step, index))
        {
            continue;
        }

        hr = PartitionAccess::Extract (bytes, name, index, part, error);

        if (FAILED (hr))
        {
            continue;
        }

        WalkBytes (path + ContainerPath::kPathSeparator + entry.step, entry.step, std::move (part), depth + 1, visitor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkDisk
//
//  One disk's row, with every file on it read, then the images stored as
//  files on it.
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkDisk (const std::string & path, const std::string & name, const std::vector<Byte> & sectors, const SectorDecodeReport & report, VolumeKind kind, int depth, const Visitor & visitor)
{
    CollectionDisk                                         disk;
    DiskIdentity::Volume                                   identity;
    VolumeListing                                          listing;
    std::unique_ptr<IVolume>                               volume;
    std::vector<std::pair<std::string, std::vector<Byte>>> inner;
    HRESULT                                                hr = S_OK;



    hr = DiskIdentity::ComputeVolume (sectors, report, identity);
    IGNORE_RETURN_VALUE (hr, S_OK);

    disk.path       = path;
    disk.container  = GetContainerName (name);
    disk.fileSystem = VolumeImage::GetSystemName (kind);
    disk.sha256     = identity.sha256;
    disk.crc32      = identity.crc32;
    disk.hasMore    = identity.hasMore;

    volume = (kind == VolumeKind::Unknown) ? nullptr : VolumeImage::CreateVolume (kind, sectors);
    hr     = (volume != nullptr) ? volume->Enumerate (listing) : E_FAIL;

    if (SUCCEEDED (hr))
    {
        disk.volumeName = GetVolumeName (listing);
        disk.freeBytes  = (uint64_t) listing.freeUnits * CatalogModel::GetUnitBytes (kind);

        WalkDirectory (*volume, kind, std::string(), disk, inner, visitor);
    }

    if (visitor.onDisk)
    {
        visitor.onDisk (std::move (disk));
    }

    for (auto & [innerPath, bytes] : inner)
    {
        WalkBytes (path + ContainerPath::kPathSeparator + innerPath, GetLeaf (innerPath), std::move (bytes), depth + 1, visitor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::WalkDirectory
//
////////////////////////////////////////////////////////////////////////////////

void CollectionWalk::WalkDirectory (const IVolume & volume, VolumeKind kind, const std::string & directory, CollectionDisk & ioDisk, std::vector<std::pair<std::string, std::vector<Byte>>> & ioInner, const Visitor & visitor)
{
    VolumeListing  listing;
    HRESULT        hr = S_OK;



    hr = directory.empty() ? volume.Enumerate (listing) : volume.EnumerateDirectory (FilePath::Parse (directory), listing);

    if (FAILED (hr))
    {
        return;
    }

    for (const FileEntry & entry : listing.entries)
    {
        std::string     entryPath = directory.empty() ? entry.name : directory + "/" + entry.name;
        FilePath        filePath  = (kind == VolumeKind::Dos33) ? FilePath::FromName (entry.name).WithLeafIndex (entry.catalogIndex) : FilePath::Parse (entryPath);
        FilePayload     payload;
        CollectionFile  file;

        if (IsStopped (visitor))
        {
            return;
        }

        if (entry.isDirectory)
        {
            WalkDirectory (volume, kind, entryPath, ioDisk, ioInner, visitor);
            continue;
        }

        ioDisk.files++;

        hr = volume.Read (filePath, payload);

        if (FAILED (hr))
        {
            continue;
        }

        file.path  = entryPath;
        file.type  = entry.type;
        file.bytes = payload.bytes.size();

        hr = DiskIdentity::ComputeFile (payload.bytes, file.sha256);
        IGNORE_RETURN_VALUE (hr, S_OK);

        ioDisk.fileList.push_back (file);

        if (visitor.onFile)
        {
            visitor.onFile (ioDisk, entryPath, entry, kind, payload);
        }

        if (IsContainerName (entry.name) || entry.type == s_kArchiveType)
        {
            ioInner.emplace_back (entryPath, std::move (payload.bytes));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::IsContainerName
//
////////////////////////////////////////////////////////////////////////////////

bool CollectionWalk::IsContainerName (const std::string & name)
{
    return HasExtension (name, ".zip") || HasExtension (name, ".gz") || TreeModel::IsSupportedImage (TextEncoding::NarrowToWide (name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::IsStopped
//
////////////////////////////////////////////////////////////////////////////////

bool CollectionWalk::IsStopped (const Visitor & visitor)
{
    return visitor.isStopped && visitor.isStopped();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::HasExtension
//
////////////////////////////////////////////////////////////////////////////////

bool CollectionWalk::HasExtension (const std::string & name, std::string_view extension)
{
    return name.size() > extension.size() && _strnicmp (name.c_str() + name.size() - extension.size(), extension.data(), extension.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::GetLeaf
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionWalk::GetLeaf (const std::string & path)
{
    size_t  slash = path.find_last_of ("/\\");



    return (slash == std::string::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::GetVolumeName
//
//  A ProDOS, Pascal or HFS volume's name, or a DOS 3.3 disk's volume
//  number.
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionWalk::GetVolumeName (const VolumeListing & listing)
{
    if (listing.hasVolumeName)
    {
        return listing.volumeName;
    }

    return listing.hasVolumeNumber ? std::to_string (listing.volumeNumber) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk::GetContainerName
//
//  The image file's form by its extension, as the Type column has it, with
//  a gzip's taken from the name inside it. An extension with no Type column
//  text is given in capitals.
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionWalk::GetContainerName (const std::string & name)
{
    std::string  inner = HasExtension (name, ".gz") ? name.substr (0, name.size() - 3) : name;
    std::string  type  = TextEncoding::WideToNarrow (CatalogModel::GetDiskImageTypeName (TextEncoding::NarrowToWide (inner)));
    size_t       dot   = inner.rfind ('.');



    if (!type.empty() || dot == std::string::npos)
    {
        return type;
    }

    type = inner.substr (dot + 1);
    std::transform (type.begin(), type.end(), type.begin(), [] (char c) { return (char) toupper ((unsigned char) c); });

    return type + " file";
}