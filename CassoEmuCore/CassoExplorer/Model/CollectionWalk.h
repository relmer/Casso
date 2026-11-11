#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

class IDiskFileIo;
class IVolume;
struct PartitionEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionFile
//
//  A file on a disk the walk found: its path on the volume, with '/' between
//  folders, its type and the SHA-256 of its data fork.
//
////////////////////////////////////////////////////////////////////////////////

struct CollectionFile
{
    std::string  path;
    Byte         type         = 0;
    uint64_t     bytes        = 0;
    std::string  sha256;
    size_t       catalogIndex = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionDisk
//
//  A disk image the walk found, a row of Disk list. The path is a
//  ContainerPath's text, so an image in a ZIP or on another image has the
//  ZIP and the image in it. The container is the image file's form, and
//  the bytes are the volume's.
//
////////////////////////////////////////////////////////////////////////////////

struct CollectionDisk
{
    std::string                  path;
    std::string                  container;
    std::string                  fileSystem;
    std::string                  volumeName;
    uint32_t                     files     = 0;
    uint64_t                     freeBytes = 0;
    uint32_t                     crc32     = 0;
    std::string                  sha256;
    bool                         hasMore   = false;
    uint64_t                     bytes     = 0;
    VolumeKind                   kind      = VolumeKind::Unknown;
    std::vector<CollectionFile>  fileList;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionWalk
//
//  Every disk image in a folder's subtree, and in each container on the way:
//  ZIP, gzip, NuFX and Binary II files, the partitions of a hard disk image
//  and images stored as files on other images, to kMaxDepth. Each disk is
//  reported as it is found, with every file on it read. A file is passed to
//  onFile as it is read, for a search of its contents. The walk ends early
//  when isStopped returns true.
//
//  It runs on whatever thread calls Walk; the window calls it from a
//  listing's thread.
//
////////////////////////////////////////////////////////////////////////////////

class CollectionWalk
{
public:
    struct Visitor
    {
        using DiskFn    = std::function<void (CollectionDisk && disk)>;
        using FileFn    = std::function<void (const CollectionDisk & disk, const std::string & path, const FileEntry & entry, VolumeKind kind, const FilePayload & payload)>;
        using StoppedFn = std::function<bool ()>;

        DiskFn     onDisk;
        FileFn     onFile;
        StoppedFn  isStopped;
    };

    static constexpr int       kMaxDepth     = 8;
    static constexpr uint64_t  kLargestImage = 1024ull * 1024 * 1024;

    static HRESULT  Walk      (IDiskFileIo & io, const std::string & root, const Visitor & visitor);
    static void     WalkBytes (const std::string & path, const std::string & name, std::vector<Byte> && bytes, int depth, const Visitor & visitor);

    //  Whether a file's name or type marks it as a container the walk opens.
    static bool  IsContainerName (const std::string & name);

private:
    static void         WalkFolder       (IDiskFileIo & io, const std::string & folder, const Visitor & visitor);
    static void         WalkZip          (const std::string & path, std::span<const Byte> bytes, int depth, const Visitor & visitor);
    static void         WalkArchive      (const std::string & path, const IVolume & volume, int depth, const Visitor & visitor);
    static void         WalkParts        (const std::string & path, const std::string & name, const std::vector<Byte> & bytes, const std::vector<PartitionEntry> & parts, int depth, const Visitor & visitor);
    static void         WalkDisk         (const std::string & path, const std::string & name, const std::vector<Byte> & sectors, const SectorDecodeReport & report, VolumeKind kind, int depth, const Visitor & visitor);
    static void         WalkDirectory    (const IVolume & volume, VolumeKind kind, const std::string & directory, CollectionDisk & ioDisk, std::vector<std::pair<std::string, std::vector<Byte>>> & ioInner, const Visitor & visitor);
    static bool         IsStopped        (const Visitor & visitor);
    static bool         HasExtension     (const std::string & name, std::string_view extension);
    static std::string  GetLeaf          (const std::string & path);
    static std::string  GetVolumeName    (const VolumeListing & listing);
    static std::string  GetContainerName (const std::string & name);

    static constexpr Byte    s_kArchiveType = 0xE0;
    static constexpr size_t  s_kFloppyBytes = 143360;
    static constexpr size_t  s_kBlockBytes  = 512;
};
