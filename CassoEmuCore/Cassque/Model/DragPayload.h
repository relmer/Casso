#pragma once

#include "Pch.h"

#include "Cassque/Model/HostFileNaming.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayload
//
//  What a drag offers, decided from what is being dragged and nothing else.
//
//  THE DATA OBJECT IS BUILT ELSEWHERE; this class decides its contents. A drag
//  of catalog entries offers the private format, so a drop on another Apple
//  volume copies raw, and file descriptors with the host naming rule, so a
//  drop on a host folder receives converted files rendered on demand. A drag
//  of disk images offers the paths themselves, which is what the emulator's
//  own drop target already accepts.
//
//  A ProDOS directory becomes a descriptor tree: the directory, then every
//  entry beneath it with a relative path, listed through a callback because
//  the volume layer does not walk into a directory and the caller may.
//
////////////////////////////////////////////////////////////////////////////////

class DragPayload
{
public:
    enum class Format { CatalogEntries, FileDescriptors, FileContents, HDrop };

    enum class SourceKind { CatalogEntries, DiskImages, HostFiles };

    //  One file the host side would receive: its name relative to the drop
    //  folder, the catalog path it is rendered from, and whether it is a
    //  folder that needs no rendering at all.
    struct Descriptor
    {
        std::wstring  relativePath;
        std::string   catalogPath;
        bool          isDirectory = false;
        bool          converted   = false;   // rendered through a listing or text conversion
    };

    struct Plan
    {
        std::vector<Format>        formats;
        std::vector<Descriptor>    descriptors;   // FileDescriptors / FileContents, by index
        std::vector<std::wstring>  hostPaths;     // HDrop
        std::string                privateBytes;  // CatalogEntries
    };

    //  Lists one directory inside the source image, for the recursion.
    using DirectoryLister = std::function<HRESULT (const std::string & directoryPath, VolumeListing & outListing)>;

    static Plan  Build (SourceKind                     source,
                        const std::string            & sourceImage,
                        VolumeKind                     kind,
                        const std::vector<FileEntry> & entries,
                        HostFileNaming::Style          style,
                        const DirectoryLister        & listDirectory,
                        const std::vector<std::wstring> & hostPaths);

    //  The private format's bytes: the image path, the file system, and one
    //  catalog path per line. Text, so a receiver in another process reads
    //  it without a struct layout to agree on.
    static std::string  EncodeCatalogEntries (const std::string & sourceImage, VolumeKind kind,
                                              const std::vector<std::string> & catalogPaths);
    static bool         DecodeCatalogEntries (const std::string & bytes, std::string & outSourceImage,
                                              VolumeKind & outKind, std::vector<std::string> & outCatalogPaths);

    //  Whether a folder can land on this file system at all.
    static bool  CanReceiveFolder (VolumeKind kind);

    //  The host name one entry gets: converted for the two BASIC types and
    //  text, raw for everything else.
    static std::wstring  GetHostName (const FileEntry & entry, VolumeKind kind, HostFileNaming::Style style, bool & outConverted);

    static constexpr const char *  kPrivateFormatName = "CassqueCatalogEntries";

private:
    static void  AppendEntry (const FileEntry              & entry,
                              VolumeKind                     kind,
                              HostFileNaming::Style          style,
                              const std::wstring           & relativeFolder,
                              const std::string            & catalogFolder,
                              const DirectoryLister        & listDirectory,
                              int                            depth,
                              std::vector<Descriptor>      & inOutDescriptors);

    static constexpr int  kMaxDirectoryDepth = 16;
};
