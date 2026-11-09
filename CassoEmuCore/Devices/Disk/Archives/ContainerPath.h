#pragma once

#include "Pch.h"

class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerStep
//
//  One container on the way to a location. The first step is a host file,
//  whose name is its UTF-8 path; a ZIP member's name is its path in the
//  ZIP, and an image's or archive's file is its path on the volume, with
//  '/' between folders. A gzip step's name is unused.
//
////////////////////////////////////////////////////////////////////////////////

struct ContainerStep
{
    enum class Kind
    {
        HostFile,
        ZipMember,
        Gzip,
        ImageFile,
        ArchiveRecord,
        Partition,
    };

    Kind         kind = Kind::HostFile;
    std::string  name;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OpenedContainer
//
//  The last step's bytes, and the file name that indicates their format by
//  its extension.
//
////////////////////////////////////////////////////////////////////////////////

struct OpenedContainer
{
    std::vector<Byte>  bytes;
    std::string        name;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath
//
//  A location inside containers at any depth: a host file, then ZIP
//  members, gzip files, files on an image and records in an archive, each
//  opened from the bytes of the step before. Replace writes back up the
//  steps: each container is rebuilt with the step after it replaced, and the
//  host file is written last, through a temporary file. Paths whose steps
//  are equal are one location.
//
//  As text, a path is the steps joined with '|', which a Windows file name
//  can't contain: `C:\Disks\games.zip|disks/a.po.gz|DOCS/README`. Each step's
//  kind follows from the step before it: a ZIP entry after a `.zip`, the
//  contents of a `.gz` with a step after it, and otherwise a file on an image
//  or a record in an archive.
//
////////////////////////////////////////////////////////////////////////////////

class ContainerPath
{
public:
    std::vector<ContainerStep>  steps;

    static constexpr char  kPathSeparator = '|';

    std::string          GetId    () const;
    std::string          ToString () const;
    const std::string &  GetHost  () const { return steps[0].name; }

    bool  operator== (const ContainerPath & other) const { return GetId() == other.GetId(); }

    static bool           IsNested (const std::string & text) { return text.find (kPathSeparator) != std::string::npos; }
    static ContainerPath  Parse    (const std::string & text);

    static HRESULT  Open         (const ContainerPath & path, IFileSystem & fs, OpenedContainer & outOpened, std::string & outError);
    static HRESULT  Replace      (const ContainerPath & path, IFileSystem & fs, std::span<const Byte> innerBytes, std::string & outError);
    static HRESULT  OpenBytes    (const ContainerPath & path, std::span<const Byte> hostBytes, OpenedContainer & outOpened, std::string & outError);
    static HRESULT  ReplaceBytes (const ContainerPath & path, std::span<const Byte> hostBytes, std::span<const Byte> innerBytes, std::vector<Byte> & outHostBytes, std::string & outError);

private:
    struct Level
    {
        std::vector<Byte>  bytes;
        std::string        name;
        std::string        gzipName;
    };

    static constexpr char  kIdSeparator = '\x1F';

    static HRESULT      OpenLevels         (const ContainerPath & path, std::span<const Byte> hostBytes, std::vector<Level> & outLevels, std::string & outError);
    static HRESULT      OpenStep           (const ContainerStep & step, const Level & parent, Level & outLevel, std::string & outError);
    static HRESULT      ReplaceStep        (const ContainerStep & step, const Level & parent, const Level & child, std::vector<Byte> & outParent, std::string & outError);
    static HRESULT      ReadVolume         (const Level & parent, const std::string & file, std::vector<Byte> & outBytes, std::string & outError);
    static HRESULT      WriteVolume        (const Level & parent, const std::string & file, std::span<const Byte> bytes, std::vector<Byte> & outParent, std::string & outError);
    static std::string  GetLeafName        (const std::string & path);
    static std::string  StripGzipExtension (const std::string & name);
    static bool         HasExtension       (const std::string & name, std::string_view extension);
};
