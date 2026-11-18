#include "Pch.h"

#include "CassoLink/FullSend/FullSendFolder.h"

#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::FullSendFolder
//
//  A chosen extension has its leading period; empty is .po for ProDOS and
//  Pascal and .dsk for any other.
//
////////////////////////////////////////////////////////////////////////////////

FullSendFolder::FullSendFolder (IDiskFileIo & fileIo, std::string path, std::string extension) :
    m_fileIo    (fileIo),
    m_path      (std::move (path)),
    m_extension (std::move (extension))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::Load
//
//  The manifest as the folder has it, under its old name when only that is
//  there, which is renamed. A manifest that cannot be parsed is never
//  overwritten, and the folder is damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::Load()
{
    HRESULT                      hr         = S_OK;
    std::string                  path       = GetPath (FullSendManifest::kFileName);
    std::string                  legacy     = GetPath (FullSendManifest::kLegacyFileName);
    std::lock_guard<std::mutex>  folderLock { GetLock() };
    bool                         exists     = m_fileIo.Exists (path);



    if (!exists && m_fileIo.Exists (legacy))
    {
        hr = m_fileIo.RenameWithoutReplacing (legacy, path);
        CHRF (hr, m_damaged = true);

        exists = true;
    }

    BAIL_OUT_IF (!exists, S_OK);

    hr = ReadManifest (path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::Reload
//
//  The manifest as it is in the folder now, read again under the folder's lock
//  before a disk is checked or sent. Two Apples can send to one folder at once,
//  and numbering a disk and recognizing a repeat both use the rows the other
//  session added since.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::Reload()
{
    HRESULT      hr     = S_OK;
    std::string  path   = GetPath (FullSendManifest::kFileName);
    bool         exists = m_fileIo.Exists (path);



    BAIL_OUT_IF (!exists, S_OK);

    hr = ReadManifest (path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::ReadManifest
//
//  The manifest in memory is replaced only by one that parses.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::ReadManifest (const std::string & path)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  bytes;
    FullSendManifest   manifest;



    hr = m_fileIo.ReadAllBytes (path, bytes);
    CHRF (hr, m_damaged = true);

    hr = manifest.Parse (bytes);
    CHRF (hr, m_damaged = true);

    m_manifest = std::move (manifest);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::WriteManifest
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::WriteManifest()
{
    HRESULT               hr = S_OK;
    CommitPlan::Progress  progress;



    hr = DurableCommit::Commit (m_fileIo, GetPath (FullSendManifest::kFileName), m_manifest.Format(), m_commitTag++, CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::WriteImage
//
//  CreateNew, so an existing file is never overwritten. When a file from another
//  session or the user is already at the title, the next number in parentheses
//  is added. A raw capture, withRaw, is written as the image, a WOZ; its
//  reads, when there are any, go beside it with kReadsSuffix added. A sector
//  image's list of unreadable sectors goes beside it, as for Read disk to
//  image. A write-protected disk's image is write-protected: a WOZ by its
//  INFO chunk, and a sector image by the file's read-only attribute. A title
//  that writes to its disk to test for a copy runs only from such an image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::WriteImage (const std::string & title, const FullSendImage & image, bool withRaw, std::string & outFile)
{
    HRESULT               hr        = S_OK;
    bool                  isRaw     = withRaw && !image.raw.empty();
    bool                  isWoz     = isRaw || !image.report.layouts.empty();
    std::string           extension = isWoz ? std::string (kRawExtension) : FullSendDisk::GetImageExtension (image.sectors, m_extension);
    std::vector<Byte>     bytes;
    CommitPlan::Progress  progress;



    hr = BuildImageFile (image, isRaw, extension, bytes);
    CHR (hr);

    outFile = FindFreeName (title, extension);

    hr = DurableCommit::Commit (m_fileIo, GetPath (outFile), bytes, m_commitTag++, CommitMode::CreateNew, progress);
    CHR (hr);

    if (image.writeProtected && extension != kRawExtension)
    {
        hr = m_fileIo.SetReadOnly (GetPath (outFile), true);
        CHR (hr);
    }

    hr = WriteBeside (outFile, image, isRaw);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::BuildImageFile
//
//  The file's bytes: the raw capture, a WOZ built to the layouts of a disk
//  whose tracks the link measured, or the sectors in the extension's
//  container.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::BuildImageFile (const FullSendImage & image, bool withRaw, const std::string & extension, std::vector<Byte> & outBytes)
{
    HRESULT     hr     = S_OK;
    DiskFormat  format = DiskFormat::Dsk;



    if (withRaw)
    {
        outBytes = image.raw;
    }
    else if (!image.report.layouts.empty())
    {
        hr = TrackLayoutWriter::BuildWoz (image.sectors, image.report.layouts, outBytes);
        CHR (hr);
    }
    else
    {
        hr = DiskImageStore::GetSourceFormatByExtension (extension, format);
        CHR (hr);

        hr = BlankDiskBuilder::WrapInContainer (format, NibbleImageCodec::GetTrackSizeForPath (extension), false, 0, image.sectors, outBytes);
        CHR (hr);
    }

    if (image.writeProtected && extension == kRawExtension)
    {
        hr = WozLoader::SetWriteProtectFlag (outBytes, true);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::WriteBeside
//
//  The files that go beside an image: a raw capture's reads, or a sector
//  image's list of the sectors that weren't clean.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::WriteBeside (const std::string & file, const FullSendImage & image, bool withRaw)
{
    HRESULT               hr        = S_OK;
    bool                  hasReads  = withRaw && !image.reads.empty();
    bool                  hasList   = !withRaw && (!image.unreadable.empty() || !image.report.marks.empty());
    std::string           text;
    CommitPlan::Progress  progress;



    if (hasReads)
    {
        hr = DurableCommit::Commit (m_fileIo, GetPath (file + kReadsSuffix), image.reads, m_commitTag++, CommitMode::Replace, progress);
        CHR (hr);
    }

    if (hasList)
    {
        text = CassoLinkDiskTransfer::FormatUnreadableList (image.unreadable, image.report.marks);

        hr = DurableCommit::Commit (m_fileIo, CassoLinkDiskTransfer::GetUnreadableListPath (GetPath (file)), std::vector<Byte> (text.begin(), text.end()), m_commitTag++, CommitMode::Replace, progress);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::FindFreeName
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendFolder::FindFreeName (const std::string & title, const std::string & extension)
{
    std::string  file = title + extension;



    for (int copy = 2; m_fileIo.Exists (GetPath (file)); copy++)
    {
        file = std::format ("{} ({}){}", title, copy, extension);
    }

    return file;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::FindMovedImages
//
//  Uploads are saved as numbered files and are likely to be renamed. Each row
//  that matches and whose image is missing from the folder is assigned a
//  different one of the folder's unlisted images with its hash, in sequence
//  and file name order. A row whose image is in the folder, or that has no
//  image left, isn't changed. A duplicateOf with the old name takes the new
//  one too. outRenames has each old name and new name;
//  the manifest is changed in memory only.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::FindMovedImages (const RowMatch & matches, Renames & outRenames)
{
    HRESULT                     hr   = S_OK;
    std::vector<FullSendRow> &  rows = m_manifest.GetRows();
    std::string                 old;



    outRenames.clear();

    for (FullSendRow & row : rows)
    {
        if (!matches (row) || row.file.empty() || m_fileIo.Exists (GetPath (row.file)))
        {
            continue;
        }

        hr = HashUnlisted();
        CHR (hr);

        auto  found = std::find_if (m_unlisted.begin(), m_unlisted.end(), [&] (const auto & image) { return image.second == row.hash; });

        if (found == m_unlisted.end())
        {
            continue;
        }

        old      = row.file;
        row.file = found->first;

        m_unlisted.erase (found);

        for (FullSendRow & other : rows)
        {
            if (other.duplicateOf == old)
            {
                other.duplicateOf = row.file;
            }
        }

        outRenames.push_back ({ old, row.file });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::HashUnlisted
//
//  Each file in the folder that the manifest doesn't list and that loads as a
//  disk image, hashed as its sectors were when it was read: in DOS order, so
//  a .po or .woz has the hash of the disk in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendFolder::HashUnlisted()
{
    HRESULT                   hr     = S_OK;
    HRESULT                   hrFile = S_OK;
    std::vector<std::string>  names;
    std::vector<Byte>         bytes;
    std::vector<Byte>         sectors;
    SectorDecodeReport        report;
    std::string               hash;



    BAIL_OUT_IF (m_unlistedRead, S_OK);

    m_unlistedRead = true;

    hr = m_fileIo.ListFiles (m_path, names);
    CHR (hr);

    std::sort (names.begin(), names.end());

    for (const std::string & name : names)
    {
        bool  listed = std::any_of (m_manifest.GetRows().begin(), m_manifest.GetRows().end(), [&] (const FullSendRow & row) { return row.file == name; });

        if (listed || name == FullSendManifest::kFileName || name == FullSendManifest::kLegacyFileName)
        {
            continue;
        }

        //  A file that doesn't load as a disk image isn't one of the images.
        hrFile = m_fileIo.ReadAllBytes (GetPath (name), bytes);
        hrFile = SUCCEEDED (hrFile) ? VolumeImage::Load (bytes, GetPath (name), sectors, report) : hrFile;
        hrFile = SUCCEEDED (hrFile) ? FullSendDisk::ComputeHash (sectors, hash) : hrFile;

        if (SUCCEEDED (hrFile))
        {
            m_unlisted.push_back ({ name, hash });
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::HasImage
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendFolder::HasImage (const FullSendRow & row)
{
    return !row.file.empty() && m_fileIo.Exists (GetPath (row.file));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::FindWithImage
//
//  The first matching row whose image is in the folder, or nullptr.
//
////////////////////////////////////////////////////////////////////////////////

const FullSendRow * FullSendFolder::FindWithImage (const RowMatch & matches)
{
    const std::vector<FullSendRow> &  rows = m_manifest.GetRows();



    auto  found = std::find_if (rows.begin(), rows.end(), [&] (const FullSendRow & row) { return matches (row) && HasImage (row); });



    return found == rows.end() ? nullptr : &*found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendFolder::GetPath (const std::string & file) const
{
    return (std::filesystem::path (m_path) / file).string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolder::GetLock
//
//  The lock every session in the process takes around a change to a folder's
//  manifest and images, also taken by a rename in the folder. There is one
//  for each folder, by its path in lowercase.
//
////////////////////////////////////////////////////////////////////////////////

std::mutex & FullSendFolder::GetLock (const std::string & folder)
{
    static std::mutex                                          tableLock;
    static std::map<std::string, std::unique_ptr<std::mutex>>  locks;
    std::lock_guard<std::mutex>                                guard (tableLock);
    std::string                                                key   = folder;



    std::transform (key.begin(), key.end(), key.begin(), [] (unsigned char ch) { return (char) std::tolower (ch); });

    if (!locks[key])
    {
        locks[key] = std::make_unique<std::mutex>();
    }

    return *locks[key];
}
