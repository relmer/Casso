#include "Pch.h"

#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::GetId
//
//  Each step's kind and name, joined with a separator no name contains.
//
////////////////////////////////////////////////////////////////////////////////

std::string ContainerPath::GetId() const
{
    std::string  id;



    for (const ContainerStep & step : steps)
    {
        id += (char) ('0' + (int) step.kind);
        id += step.name;
        id += kIdSeparator;
    }

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::ToString
//
////////////////////////////////////////////////////////////////////////////////

std::string ContainerPath::ToString() const
{
    std::string  text;



    for (const ContainerStep & step : steps)
    {
        if (step.kind == ContainerStep::Kind::Gzip)
        {
            continue;
        }

        if (!text.empty())
        {
            text += kPathSeparator;
        }

        text += step.name;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::Parse
//
//  A gzip step goes in when a name ending `.gz` has a step after it; a
//  gzip at the end is expanded by the image loader, as one on the PC is.
//
//  A partition of a card in a reader is the host step, card and partition
//  together, as it is read and written in place and the card is never read
//  whole.
//
////////////////////////////////////////////////////////////////////////////////

ContainerPath ContainerPath::Parse (const std::string & text)
{
    ContainerPath  path;
    size_t         start     = 0;
    size_t         bar       = 0;
    int            partition = 0;
    bool           isOnCard  = false;
    std::string    prior;



    while (start <= text.size())
    {
        std::string  name;

        bar  = text.find (kPathSeparator, start);
        name = text.substr (start, (bar == std::string::npos) ? std::string::npos : bar - start);

        isOnCard = path.steps.size() == 1 && PhysicalCards::IsCardPath (path.steps[0].name) && PartitionAccess::TryParseStepName (name, partition);

        if (path.steps.empty())
        {
            path.steps.push_back ({ ContainerStep::Kind::HostFile, name });
        }
        else if (isOnCard)
        {
            path.steps[0].name += kPathSeparator;
            path.steps[0].name += name;
        }
        else
        {
            if (HasExtension (prior, ".gz"))
            {
                path.steps.push_back ({ ContainerStep::Kind::Gzip, std::string() });
                prior = StripGzipExtension (prior);
            }

            if (HasExtension (prior, ".zip"))
            {
                path.steps.push_back ({ ContainerStep::Kind::ZipMember, name });
            }
            else if (PartitionAccess::TryParseStepName (name, partition))
            {
                path.steps.push_back ({ ContainerStep::Kind::Partition, name });
            }
            else
            {
                path.steps.push_back ({ ContainerStep::Kind::ImageFile, name });
            }
        }

        prior = name;

        if (bar == std::string::npos)
        {
            break;
        }

        start = bar + 1;
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::Open (const ContainerPath & path, IFileSystem & fs, OpenedContainer & outOpened, std::string & outError)
{
    HRESULT      hr       = S_OK;
    bool         isRooted = !path.steps.empty();
    std::string  content;



    outOpened = {};

    CBRAEx (isRooted, E_INVALIDARG);

    hr = fs.ReadAllText (TextEncoding::Utf8ToWide (path.GetHost()), content);
    CHRF (hr, outError = std::format ("{} couldn't be read.", GetLeafName (path.GetHost())));

    hr = OpenBytes (path, std::span<const Byte> ((const Byte *) content.data(), content.size()), outOpened, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::Replace
//
//  The host file is written once every step is rebuilt, so a failure at any
//  step returns before it is written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::Replace (const ContainerPath & path, IFileSystem & fs, std::span<const Byte> innerBytes, std::string & outError)
{
    HRESULT            hr       = S_OK;
    bool               isRooted = !path.steps.empty();
    std::string        content;
    std::vector<Byte>  rebuilt;



    CBRAEx (isRooted, E_INVALIDARG);

    hr = fs.ReadAllText (TextEncoding::Utf8ToWide (path.GetHost()), content);
    CHRF (hr, outError = std::format ("{} couldn't be read.", GetLeafName (path.GetHost())));

    hr = ReplaceBytes (path, std::span<const Byte> ((const Byte *) content.data(), content.size()), innerBytes, rebuilt, outError);
    CHR (hr);

    content.assign (rebuilt.begin(), rebuilt.end());

    hr = fs.WriteAllText (TextEncoding::Utf8ToWide (path.GetHost()), content);
    CHRF (hr, outError = std::format ("{} couldn't be written.", GetLeafName (path.GetHost())));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::OpenBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::OpenBytes (const ContainerPath & path, std::span<const Byte> hostBytes, OpenedContainer & outOpened, std::string & outError)
{
    HRESULT             hr = S_OK;
    std::vector<Level>  levels;



    outOpened = {};

    hr = OpenLevels (path, hostBytes, levels, outError);
    CHR (hr);

    outOpened.bytes = std::move (levels.back().bytes);
    outOpened.name  = levels.back().name;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::ReplaceBytes
//
//  Each step is opened first, so that every container is rebuilt from the
//  bytes it has now.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::ReplaceBytes (const ContainerPath & path, std::span<const Byte> hostBytes, std::span<const Byte> innerBytes, std::vector<Byte> & outHostBytes, std::string & outError)
{
    HRESULT             hr = S_OK;
    std::vector<Level>  levels;
    std::vector<Byte>   rebuilt;



    outHostBytes.clear();

    hr = OpenLevels (path, hostBytes, levels, outError);
    CHR (hr);

    levels.back().bytes.assign (innerBytes.begin(), innerBytes.end());

    for (size_t i = levels.size() - 1; i > 0; i--)
    {
        hr = ReplaceStep (path.steps[i], levels[i - 1], levels[i], rebuilt, outError);
        CHR (hr);

        levels[i - 1].bytes = std::move (rebuilt);
    }

    outHostBytes = std::move (levels[0].bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::OpenLevels
//
//  The bytes at every step, the host file's first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::OpenLevels (const ContainerPath & path, std::span<const Byte> hostBytes, std::vector<Level> & outLevels, std::string & outError)
{
    HRESULT  hr       = S_OK;
    bool     isRooted = !path.steps.empty() && path.steps[0].kind == ContainerStep::Kind::HostFile;
    Level    level;



    outLevels.clear();

    CBRAEx (isRooted, E_INVALIDARG);

    level.bytes.assign (hostBytes.begin(), hostBytes.end());
    level.name = path.steps[0].name;
    outLevels.push_back (std::move (level));

    for (size_t i = 1; i < path.steps.size(); i++)
    {
        Level  next;

        hr = OpenStep (path.steps[i], outLevels.back(), next, outError);
        CHR (hr);

        outLevels.push_back (std::move (next));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::OpenStep
//
//  One step's bytes from the step before. A gzip file's inner name is the
//  outer one without `.gz`, as for an image opened from a gzip on the PC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::OpenStep (const ContainerStep & step, const Level & parent, Level & outLevel, std::string & outError)
{
    HRESULT                hr          = S_OK;
    bool                   found       = false;
    bool                   isPartition = false;
    int                    partition   = 0;
    std::vector<ZipEntry>  entries;



    outLevel = {};

    switch (step.kind)
    {
        case ContainerStep::Kind::ZipMember:
            hr = ZipArchive::Extract (parent.bytes, entries);
            CHRF (hr, outError = std::format ("{} doesn't open as a ZIP file.", GetLeafName (parent.name)));

            for (ZipEntry & entry : entries)
            {
                if (!found && entry.path == step.name)
                {
                    found          = true;
                    outLevel.bytes = std::move (entry.data);
                }
            }

            CBRFEx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), outError = std::format ("{} isn't in {}.", step.name, GetLeafName (parent.name)));

            outLevel.name = step.name;
            break;

        case ContainerStep::Kind::Gzip:
            hr = GzipFile::Expand (parent.bytes, outLevel.bytes, outLevel.gzipName, outError);
            CHR (hr);

            outLevel.name = StripGzipExtension (parent.name);
            break;

        case ContainerStep::Kind::ImageFile:
        case ContainerStep::Kind::ArchiveRecord:
            hr = ReadVolume (parent, step.name, outLevel.bytes, outError);
            CHR (hr);

            outLevel.name = step.name;
            break;

        case ContainerStep::Kind::Partition:
            isPartition = PartitionAccess::TryParseStepName (step.name, partition);
            CBRAEx (isPartition, E_INVALIDARG);

            hr = PartitionAccess::Extract (parent.bytes, parent.name, partition, outLevel.bytes, outError);
            CHR (hr);

            outLevel.name = step.name;
            break;

        default:
            CBRAEx (false, E_INVALIDARG);
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::ReplaceStep
//
//  The step before, rebuilt with this step's new bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::ReplaceStep (const ContainerStep & step, const Level & parent, const Level & child, std::vector<Byte> & outParent, std::string & outError)
{
    HRESULT  hr          = S_OK;
    bool     isPartition = false;
    int      partition   = 0;



    outParent.clear();

    switch (step.kind)
    {
        case ContainerStep::Kind::ZipMember:
            hr = ZipArchive::Replace (parent.bytes, step.name, child.bytes, outParent);
            CHRF (hr, outError = std::format ("{} couldn't be rebuilt.", GetLeafName (parent.name)));
            break;

        case ContainerStep::Kind::Gzip:
            GzipFile::Build (child.bytes, child.gzipName, outParent);
            break;

        case ContainerStep::Kind::ImageFile:
        case ContainerStep::Kind::ArchiveRecord:
            hr = WriteVolume (parent, step.name, child.bytes, outParent, outError);
            CHR (hr);
            break;

        case ContainerStep::Kind::Partition:
            isPartition = PartitionAccess::TryParseStepName (step.name, partition);
            CBRAEx (isPartition, E_INVALIDARG);

            hr = PartitionAccess::Replace (parent.bytes, parent.name, partition, child.bytes, outParent, outError);
            CHR (hr);
            break;

        default:
            CBRAEx (false, E_INVALIDARG);
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::ReadVolume
//
//  A file's data fork from the image or archive in `parent`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::ReadVolume (const Level & parent, const std::string & file, std::vector<Byte> & outBytes, std::string & outError)
{
    HRESULT                   hr     = S_OK;
    std::vector<Byte>         sectors;
    SectorDecodeReport        report;
    VolumeKind                kind   = VolumeKind::Unknown;
    std::unique_ptr<IVolume>  volume;
    FilePayload               payload;
    std::string               leaf   = GetLeafName (parent.name);



    outBytes.clear();

    hr = VolumeImage::Load (parent.bytes, parent.name, sectors, report);
    CHRF (hr, outError = std::format ("{} doesn't open as a disk image or an archive.", leaf));

    kind   = VolumeImage::DetectFilesystem (sectors);
    volume = VolumeImage::CreateVolume (kind, sectors);
    CBRFEx (volume != nullptr, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), outError = std::format ("{} has no file system Casso reads.", leaf));

    hr = volume->Read (FilePath::Parse (file), payload);
    CHRF (hr, outError = std::format ("{} isn't on {}.", file, leaf));

    outBytes = std::move (payload.bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::WriteVolume
//
//  The file is deleted and written again with its type, dates, access and
//  resource fork, and the image saved in the form it was read in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerPath::WriteVolume (const Level & parent, const std::string & file, std::span<const Byte> bytes, std::vector<Byte> & outParent, std::string & outError)
{
    HRESULT                   hr        = S_OK;
    std::vector<Byte>         sectors;
    std::vector<Byte>         deleted;
    std::vector<Byte>         edited;
    SectorDecodeReport        report;
    VolumeKind                kind      = VolumeKind::Unknown;
    std::unique_ptr<IVolume>  volume;
    std::unique_ptr<IVolume>  rewritten;
    FilePayload               payload;
    FilePath                  path      = FilePath::Parse (file);
    std::string               leaf      = GetLeafName (parent.name);
    std::string               reason;



    outParent.clear();

    hr = VolumeImage::Load (parent.bytes, parent.name, sectors, report);
    CHRF (hr, outError = std::format ("{} doesn't open as a disk image or an archive.", leaf));

    kind   = VolumeImage::DetectFilesystem (sectors);
    volume = VolumeImage::CreateVolume (kind, sectors);
    CBRFEx (volume != nullptr, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), outError = std::format ("{} has no file system Casso reads.", leaf));

    hr = volume->Read (path, payload);
    CHRF (hr, outError = std::format ("{} isn't on {}.", file, leaf));

    payload.bytes.assign (bytes.begin(), bytes.end());

    hr = volume->Delete (path, deleted);
    CHRF (hr, outError = std::format ("{} couldn't be replaced on {}.", file, leaf));

    rewritten = VolumeImage::CreateVolume (kind, deleted);
    CBRFEx (rewritten != nullptr, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), outError = std::format ("{} has no file system Casso reads.", leaf));

    hr = rewritten->Write (path, payload, edited);
    CHRF (hr, outError = std::format ("{} doesn't fit on {}.", file, leaf));

    hr = VolumeImage::Save (parent.bytes, parent.name, edited, outParent, reason);
    CHRF (hr, outError = reason);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::GetLeafName
//
//  The part of a path after its last separator, '/' or '\\'.
//
////////////////////////////////////////////////////////////////////////////////

std::string ContainerPath::GetLeafName (const std::string & path)
{
    size_t  slash = path.find_last_of ("/\\");



    return (slash == std::string::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::StripGzipExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string ContainerPath::StripGzipExtension (const std::string & name)
{
    static constexpr std::string_view  kExtension = ".gz";



    return HasExtension (name, kExtension) ? name.substr (0, name.size() - kExtension.size()) : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPath::HasExtension
//
//  Whether a name ends with the extension, in any case, after at least one
//  other character.
//
////////////////////////////////////////////////////////////////////////////////

bool ContainerPath::HasExtension (const std::string & name, std::string_view extension)
{
    return name.size() > extension.size() && _stricmp (name.c_str() + name.size() - extension.size(), extension.data()) == 0;
}
