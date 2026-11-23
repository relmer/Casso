#include "Pch.h"

#include "DiskFileCommands.h"
#include "DiskImageSession.h"
#include "IDiskFileIo.h"
#include "DiskCommandText.h"
#include "DiskFilePayload.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Devices/Disk/BlockBuffer.h"
#include "Devices/Disk/Partitions/CardVolumeEdit.h"
#include "Core/AppleSingleCodec.h"
#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "FileTypes/FileFormatTable.h"
#include "Utils.h"
#include "Devices/Disk/BlockVolumeOpener.h"
#include "Devices/Disk/DiskRepairCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunMkdir
//
//  Every directory along the path is made where it is not there yet, which is
//  what cmd's own mkdir does: `mkdir A/B/C` makes all three. A path that is
//  there in full already is an error rather than a quiet success, since the
//  caller asked for something to be made.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunMkdir (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           named    = !options.disk.path.empty();
    bool                           hasDirs  = false;
    bool                           madeAny  = false;
    size_t                         i        = 0;
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    typed;
    vector<Byte>                   staged;
    VolumeKind                     kind     = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardStaged;
    bool                           isCard   = false;



    CBRFEx (named, E_INVALIDARG, m_context.ReportMissingParameter ("<path>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_context.GetFileIo(), options.disk.imagePath);

    if (!isCard)
    {
        hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hasDirs = HasDirectories (kind);

    CBRFEx (hasDirs, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (kind))));

    hr = ResolveVolumePath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), options.disk.path, typed, result);
    CHR (hr);

    path   = FilePath::Parse (typed);
    staged = opened.sectors;

    for (i = 1; i <= path.GetDepth(); i++)
    {
        std::unique_ptr<IVolume>       over    = !isCard ? VolumeImage::CreateVolume (kind, staged) : madeAny ? card.OpenEdited (cardStaged) : nullptr;
        const IVolume                & step    = over != nullptr ? *over : card.GetVolume();
        VolumeListing                  listing;
        std::string                    prefix;
        vector<Byte>                   next;
        BlockBuffer                    cardNext;
        HRESULT                        probe   = S_OK;
        size_t                         j       = 0;
        bool                           there   = false;

        for (j = 0; j < i; j++)
        {
            prefix += (prefix.empty() ? "" : "/") + path.GetComponents()[j];
        }

        probe = step.EnumerateDirectory (FilePath::Parse (prefix), listing);
        there = SUCCEEDED (probe);

        if (there)
        {
            continue;
        }

        hr = isCard ? step.CreateDirectory (FilePath::Parse (prefix), cardNext) : step.CreateDirectory (FilePath::Parse (prefix), next);

        CHRF (hr, result.FailAction (options.disk.imagePath, "create " + prefix,
                              m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

        staged     = next;
        cardStaged = cardNext;
        madeAny    = true;
    }

    CBRFEx (madeAny, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path, "it is already on this volume"));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardStaged, result) : m_context.GetSession().SaveAndCommit (opened, staged, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::HasDirectories
//
//  The file systems mkdir and rmdir change: ProDOS and HFS. The others
//  have no directories, or are read-only.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFileCommands::HasDirectories (VolumeKind kind)
{
    return kind == VolumeKind::ProDos || kind == VolumeKind::Hfs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunRmdir
//
//  PowerShell's meanings: a directory holding anything needs --recurse, and
//  --force takes the locked entries with it.
//
//  THE LIST COMES BEFORE THE QUESTION. A subtree removal is answered for by
//  whoever typed it, and nobody can answer for what they cannot see, so every
//  entry that would go is printed with the locked ones marked and the blocks
//  totaled, and only then is the question asked. --yes answers it in advance,
//  and with nobody at the keyboard the removal is turned down.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunRmdir (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr          = S_OK;
    bool                           named       = !options.disk.path.empty();
    bool                           hasDirs     = false;
    bool                           holdsThings = false;
    bool                           permitted   = false;
    bool                           confirmed   = false;
    DiskImageSession::OpenedImage  opened;
    DirectoryRemovalPlan           plan;
    DeleteOutcome                  outcome;
    FilePath                       path;
    vector<Byte>                   edited;
    std::string                    typed;
    std::string                    declined;
    char                           note[128]   = {};
    bool                           isCard      = false;
    VolumeKind                     kind        = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;



    CBRFEx (named, E_INVALIDARG, m_context.ReportMissingParameter ("<path>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_context.GetFileIo(), options.disk.imagePath);

    if (!isCard)
    {
        hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hasDirs = HasDirectories (kind);

    CBRFEx (hasDirs, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (kind))));

    hr = ResolveVolumePath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), options.disk.path, typed, result);
    CHR (hr);

    path = FilePath::Parse (typed);

    if (isCard)
    {
        hr = card.GetVolume().BuildRemovalPlan (path, plan);
    }
    else
    {
        hr = VolumeImage::CreateVolume (kind, opened.sectors)->BuildRemovalPlan (path, plan);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    holdsThings = plan.entries.size() > 1;
    permitted   = !holdsThings || options.disk.recurse;

    CBRFEx (permitted, HRESULT_FROM_WIN32 (ERROR_DIR_NOT_EMPTY),
            result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                               m_context.ApplyPrefix ("it is not empty, and --recurse removes its entries with it")));

    if (holdsThings)
    {
        result.output += "Removing " + options.disk.path + ":\n";

        for (const DirectoryRemovalEntry & entry : plan.entries)
        {
            result.output += "  " + entry.path + (entry.isLocked ? "   (locked)" : "") + "\n";
        }

        snprintf (note, sizeof (note), "%u entries, %u blocks\n",
                  (unsigned) plan.entries.size(), (unsigned) plan.blocksFreed);

        result.output += note;
    }

    confirmed = options.disk.yes;
    declined  = "was not removed: nobody was there to answer, and --yes answers in advance";

    if (!confirmed && m_context.CanConfirm())
    {
        confirmed = m_context.Confirm ("Remove " + options.disk.path + " and everything below it?");
        declined  = "was not removed";
    }

    CBRFEx (confirmed, HRESULT_FROM_WIN32 (ERROR_CANCELLED),
            result.Fail (options.disk.imagePath, options.disk.path, m_context.ApplyPrefix (declined)));

    if (isCard)
    {
        hr = card.GetVolume().RemoveDirectory (path, options.disk.force, cardEdited, outcome);
    }
    else
    {
        hr = VolumeImage::CreateVolume (kind, opened.sectors)->RemoveDirectory (path, options.disk.force, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_context.GetSession().SaveAndCommit (opened, edited, result);
    CHR (hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, options.disk.path, warning) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunGet
//
//  Extraction goes to a named file when one is given and to the process's own
//  output otherwise, which is what makes it pipeable. Both destinations go
//  through the seam so the platform's opinion about binary output is settled in
//  one place rather than at each call site.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunGet (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.path.empty();
    VolumeKind                     kind      = VolumeKind::Unknown;
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    std::string                    name;
    char                           note[128] = {};
    std::unique_ptr<IBlockStore>   store;
    std::unique_ptr<IVolume>       blockVolume;
    CommandLineOptions             effective = options;



    CBRFEx (named, E_INVALIDARG, m_context.ReportMissingParameter ("<name>", result));

    DiskFilePayload::ApplyOutputEncoding (effective);

    //  A large ProDOS or HFS volume is read by the block, so a file is copied out
    //  without reading the volume whole. --index applies to DOS 3.3 only.
    if (!options.disk.hasIndex && BlockVolumeOpener::TryOpen (m_context.GetFileIo(), options.disk.imagePath, m_context.GetSession().GetChosenKind(), store, blockVolume, kind))
    {
        hr = ResolveEntryPath (options, kind, BlockImage (*store), path, name, result);
        CHR (hr);

        hr = blockVolume->Read (path, payload);
    }
    else
    {
        hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);

        kind = opened.kind;

        hr = ResolveEntryPath (options, kind, opened.sectors, path, name, result);
        CHR (hr);

        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->Read (path, payload);
    }

    result.failure = hr;

    CHRF (hr, result.FailAction (options.disk.imagePath, "read " + name,
                          m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = DiskFilePayload::ApplyEncoding (effective, payload, result);
    CHR (hr);

    hr = DiskFilePayload::ApplyHostFileConversion (effective, kind, name, payload, result);
    CHR (hr);

    result.resourceFork    = payload.resourceFork;
    result.finderInfo      = payload.finderInfo;
    result.hasResourceFork = payload.hasResourceFork;

    if (!options.disk.hostFile.empty())
    {
        hr = m_context.GetFileIo().WriteAllBytes (options.disk.hostFile, payload.bytes);

        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        if (payload.hasResourceFork)
        {
            hr = WriteAppleDouble (options.disk.hostFile, payload);
            CHRF (hr, result.Fail (DiskFilePayload::GetAppleDoublePath (options.disk.hostFile), "", "could not be written"));
        }
    }
    else
    {
        result.payload    = payload.bytes;
        result.hasPayload = true;
    }

    if (payload.hasLoadAddress)
    {
        snprintf (note, sizeof (note), "%s: loads at $%04X, %u bytes\n",
                  name.c_str(),
                  (unsigned) payload.loadAddress,
                  (unsigned) payload.bytes.size());

        result.diagnostics += note;
    }

    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (note, sizeof (note),
                  "%d %s could not be decoded. THIS FILE IS INCOMPLETE, "
                  "unreadable sectors were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, name, note) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunTest
//
//  Each problem the image's structure has, a line each: for a volume, the
//  units claimed twice, the units in use and marked free, the chains that
//  couldn't be followed and any catalog damage; for an archive, each record
//  whose header or data doesn't match its CRC. Units marked in use that no
//  file claims aren't listed: a DOS 3.3 disk's tracks for DOS are marked so
//  with no file on them. A clean image says so and exits clean.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunTest (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr     = S_OK;
    bool                           named  = !options.disk.imagePath.empty();
    DiskImageSession::OpenedImage  opened;
    VolumeIntegrityReport          report;
    VolumeListing                  listing;
    std::vector<std::string>       problems;



    CBRF (named, m_context.ReportMissingParameter ("<image>", result));

    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    if (opened.kind == VolumeKind::Archive)
    {
        ArchiveVolume (opened.sectors).CollectDamage (problems);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->BuildIntegrityReport (report);
        CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be checked"));

        hr = volume->Enumerate (listing);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (uint32_t unit : report.GetCrossLinked())
        {
            problems.push_back (std::format ("unit {} is claimed by more than one file", unit));
        }

        for (uint32_t unit : report.GetClaimedButFree())
        {
            problems.push_back (std::format ("unit {} is in use but marked free", unit));
        }

        for (uint16_t owner : report.GetUnfollowableChains())
        {
            problems.push_back (std::format ("file {} has a block chain that couldn't be followed", owner + 1));
        }

        problems.insert (problems.end(), listing.damage.begin(), listing.damage.end());
    }

    for (const std::string & problem : problems)
    {
        result.output += options.disk.imagePath + ": " + problem + "\n";
    }

    if (problems.empty())
    {
        result.output += options.disk.imagePath + ": no problems found\n";
    }

    result.exitStatus = problems.empty() ? DiskCommandResult::kClean : DiskCommandResult::kWithComplaints;

    if (opened.kind != VolumeKind::Archive)
    {
        DiskRepairCommand::AddVerdict (m_context.GetFileIo(), options.disk.imagePath, result);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::ReadAppleDouble
//
//  Nothing is read from a missing or unreadable `._NAME`.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::ReadAppleDouble (const CommandLineOptions & options, FilePayload & inOutPayload)
{
    HRESULT          hr       = S_OK;
    vector<Byte>     bytes;
    AppleSingleFile  single;
    std::string      error;
    bool             isDouble = false;
    bool             typed    = !options.disk.typeName.empty() || options.disk.hasLoadAddress;



    hr = m_context.GetFileIo().ReadAllBytes (DiskFilePayload::GetAppleDoublePath (options.disk.hostFile), bytes);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    isDouble = AppleSingleCodec::IsAppleDouble (bytes);
    BAIL_OUT_IF (!isDouble, S_OK);

    hr = AppleSingleCodec::Decode (bytes, single, error);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    if (single.hasProDosInfo && !typed)
    {
        inOutPayload.type       = (Byte) single.fileType;
        inOutPayload.auxType    = (Word) single.auxType;
        inOutPayload.hasAuxType = true;
    }

    inOutPayload.resourceFork    = single.resourceFork;
    inOutPayload.finderInfo      = single.finderInfo;
    inOutPayload.hasResourceFork = single.hasResourceFork;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::WriteAppleDouble
//
//  The resource fork, the Finder information and the ProDOS type of a file
//  whose data fork was just written to `hostFile`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommands::WriteAppleDouble (const std::string & hostFile, const FilePayload & payload)
{
    HRESULT          hr = S_OK;
    AppleSingleFile  single;
    vector<Byte>     bytes;



    single.resourceFork    = payload.resourceFork;
    single.finderInfo      = payload.finderInfo;
    single.hasResourceFork = true;
    single.hasProDosInfo   = true;
    single.access          = payload.access;
    single.fileType        = payload.type;
    single.auxType         = payload.auxType;

    AppleSingleCodec::EncodeDouble (single, bytes);

    hr = m_context.GetFileIo().WriteAllBytes (DiskFilePayload::GetAppleDoublePath (hostFile), bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::CommitCard
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommands::CommitCard (const std::string & imagePath, CardVolumeEdit & card, const BlockBuffer & edited, DiskCommandResult & result)
{
    std::string  refusal;
    HRESULT      hr      = card.Commit (edited, refusal);



    UNREFERENCED_PARAMETER (imagePath);

    if (FAILED (hr))
    {
        result.diagnostics += card.GetLabel() + " " + refusal + ".\n";
        result.exitStatus   = DiskCommandResult::kNoOutput;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunPut
//
//  Placing a file from the host on the disk: read it, convert it if asked, let the
//  filesystem compute the whole new volume, render that back into the container
//  and commit it.
//
//  NOTHING IS WRITTEN UNTIL EVERY DECISION HAS BEEN MADE. Each refusal below --
//  no such file on the host, a type nobody recognizes, a name the catalog cannot
//  store, a locked file, a volume with no room -- happens with the image on
//  disk untouched, because the only thing this path can commit is a finished
//  buffer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunPut (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.hostFile.empty();
    CommandLineOptions             effective = options;
    ParsedHostName                 parsed;
    bool                           hostForm  = false;
    std::string                    diskName;
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    vector<Byte>                   hostBytes;
    vector<Byte>                   edited;
    std::wstring                   importError;
    bool                           imported  = false;
    bool                           isCard    = false;
    VolumeKind                     kind      = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;
    FileStamp                      hostStamp;
    HRESULT                        hrStamp   = S_OK;



    CBRFEx (named, E_INVALIDARG, m_context.ReportMissingParameter ("<file>", result));

    hostForm = DiskFilePayload::ApplyHostFileName (effective, parsed);
    diskName = DiskFilePayload::GetOnDiskName (effective);

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_context.GetFileIo(), options.disk.imagePath);

    if (!isCard)
    {
        hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hr = ApplyUser (options, kind, diskName, result);
    CHR (hr);

    hr = m_context.GetFileIo().ReadAllBytes (options.disk.hostFile, hostBytes);

    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A conversion Casso Explorer wrote that has an import for this file
    //  system is put back in its Apple form, under the name that form has
    //  there, unless the command line has --type, --load, --text or --basic.
    imported = hostForm && parsed.converted == ParsedHostName::ConvertedKind::Format
            && options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim
            && options.disk.typeName.empty() && !options.disk.hasLoadAddress
            && HostFileNaming::CanImport (parsed.format, kind);

    if (imported)
    {
        if (options.disk.path.empty())
        {
            diskName = HostFileNaming::MakeImportName (diskName, parsed.format, kind);
        }

        hr = FileFormatTable::Import (parsed.format, kind, hostBytes, payload, importError);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, TextEncoding::WideToNarrow (importError)));
    }
    else
    {
        hr = DiskFilePayload::BuildPutPayload (effective, kind, hostBytes, payload, result);
        CHR (hr);
    }

    //  The type and address in a name Casso Explorer wrote, unless the command
    //  line has --type, --load, --text or --basic.
    if (hostForm && parsed.hasType && effective.disk.typeName.empty() && !effective.disk.hasLoadAddress
        && effective.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        hr = DiskFilePayload::ApplyHostFileType (parsed, kind, payload);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, "it has a ProDOS type that DOS 3.3 has no file type for"));
    }

    ReadAppleDouble (options, payload);

    //  A file with no Apple dates is dated when it was last written on the PC.
    if (!payload.hasDates)
    {
        hrStamp = m_context.GetFileIo().Stat (options.disk.hostFile, hostStamp);

        if (SUCCEEDED (hrStamp))
        {
            HostAppleFile::SetHostDates (hostStamp.modifiedUnix, payload);
        }
    }

    path = FilePath::Parse (diskName);

    if (isCard)
    {
        hr = card.GetVolume().Write (path, payload, cardEdited);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (kind, opened.sectors);

        hr = volume->Write (path, payload, edited);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName,
                          m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_context.GetSession().SaveAndCommit (opened, edited, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunDelete
//
//  Removal, plus the account of what it declined to do.
//
//  THE ACCOUNT IS THE POINT OF REPORTING AT ALL. A delete frees only space the
//  file uniquely owned; anything another catalog entry also claims is left
//  allocated, because freeing it would damage that other file and nothing would
//  say so until somebody read it, possibly weeks later. Leaked space is
//  recoverable at any time and a cross-linked free is not, so the asymmetry
//  runs this way -- and the user is told, on the error stream, with the command
//  still reporting that it succeeded.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunDelete (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr      = S_OK;
    bool                           named   = !options.disk.path.empty();
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    name;
    DeleteOutcome                  outcome;
    vector<Byte>                   edited;
    bool                           isCard  = false;
    VolumeKind                     kind    = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;



    CBRFEx (named, E_INVALIDARG, m_context.ReportMissingParameter ("<name>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_context.GetFileIo(), options.disk.imagePath);

    if (!isCard)
    {
        hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hr = ResolveEntryPath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), path, name, result);
    CHR (hr);

    if (isCard)
    {
        hr = card.GetVolume().Delete (path, cardEdited, outcome);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (kind, opened.sectors);

        hr = volume->Delete (path, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "delete " + name,
                          m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_context.GetSession().SaveAndCommit (opened, edited, result);
    CHR (hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, name, warning) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::RunKrunch
//
//  All or nothing: the moved files and the new directory reach the image in one
//  commit, or the image stays as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileCommands::RunKrunch (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           isPascal = false;
    int                            moved    = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   edited;



    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    isPascal = opened.kind == VolumeKind::Pascal;

    CBRFEx (isPascal, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "krunch the disk", "krunch applies to Apple Pascal disks, the only file system here that needs its free space combined"));

    hr = PascalVolume (opened.sectors).Krunch (edited, moved);

    CHRF (hr, result.FailAction (options.disk.imagePath, "krunch the disk",
                                 (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)) ? std::string ("the directory is damaged, so moving files could overwrite one")
                                                                                 : DiskCommandText::DescribeVolumeRefusal (hr, opened.kind)));

    if (moved > 0)
    {
        hr = m_context.GetSession().SaveAndCommit (opened, edited, result);
        CHR (hr);
    }

    result.output += std::format ("{}: {} {} moved\n", options.disk.imagePath, moved, Utils::GetSingularOrPluralForm (moved, "file", "files"));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::ResolveVolumePath
//
//  `/MYDISK/GAMES/CHESS` is a whole ProDOS path: the volume, then the way down
//  through it. `GAMES/CHESS` is the same place read from the volume directory.
//  The two forms are what a ProDOS user types, and the first says which disk it
//  means, so it is checked rather than dropped -- a full path naming another
//  volume is a caller acting on the wrong disk, and that is worth turning down.
//
//  A DOS 3.3 NAME IS ONE NAME, SLASHES AND ALL. That filesystem has no
//  directories and no volume names, and a catalog will hold a name with a
//  slash in it, so nothing is stripped there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommands::ResolveVolumePath (
    const CommandLineOptions             & options,
    VolumeKind                             kind,
    const BlockImage                     & sectors,
    const std::string                    & typed,
    std::string                          & outPath,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    FilePath       path      = FilePath::Parse (typed);
    bool           plain     = !path.IsRooted() || kind != VolumeKind::ProDos;
    bool           mine      = false;
    size_t         i         = 0;
    VolumeListing  listing;
    std::string    rest;
    char           note[128] = {};



    outPath = typed;

    BAIL_OUT_IF (plain, S_OK);

    {
        ProDosVolume  volume (sectors);

        hr = volume.Enumerate (listing);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

    mine = !path.IsEmpty() && _stricmp (path.GetComponents()[0].c_str(), listing.volumeName.c_str()) == 0;

    snprintf (note, sizeof (note), "is a path on another volume; this image is /%s", listing.volumeName.c_str());

    CBRFEx (mine, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND),
            result.Fail (options.disk.imagePath, typed, note));

    for (i = 1; i < path.GetComponents().size(); i++)
    {
        rest += (rest.empty() ? "" : "/") + path.GetComponents()[i];
    }

    outPath = rest;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::ApplyUser
//
//  --user as the volume layer's U:NAME form, on CP/M alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommands::ApplyUser (const CommandLineOptions & options, VolumeKind kind, std::string & inOutName, DiskCommandResult & result)
{
    HRESULT  hr    = S_OK;
    bool     isCpm = kind == VolumeKind::Cpm;



    BAIL_OUT_IF (!options.disk.hasUser, S_OK);

    CBRFEx (isCpm, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.Fail (options.disk.imagePath, "", m_context.ApplyPrefix ("%Luser means nothing on this volume: only CP/M has user numbers")));

    if (options.disk.user != 0)
    {
        inOutName = std::format ("{}:{}", options.disk.user, inOutName);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands::ResolveEntryPath
//
//  Without --index, the path is the name as typed. With it, the path is the
//  DOS 3.3 entry at that catalog position, and a name given alongside must
//  match that entry, so an outdated index fails instead of acting on a
//  different file.
//
//  --INDEX IS VALID ONLY FOR A NAME THAT SEVERAL ENTRIES SHARE. ProDOS names
//  are unique within a directory, and a DOS 3.3 name that only one entry has
//  already identifies that entry, so any other use is an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommands::ResolveEntryPath (
    const CommandLineOptions             & options,
    VolumeKind                             kind,
    const BlockImage                     & sectors,
    FilePath                             & outPath,
    std::string                          & outName,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    std::string    notAllowed = "--index is only allowed when identifying a specific file with a non-unique name";
    size_t         position  = options.disk.index - 1;
    size_t         sharing   = 0;
    bool           isDos     = kind == VolumeKind::Dos33;
    bool           inRange   = false;
    bool           matches   = true;
    bool           shared    = false;
    VolumeListing  listing;
    std::string    typed;
    char           note[128] = {};



    hr = ResolveVolumePath (options, kind, sectors, options.disk.path, typed, result);
    CHR (hr);

    hr = ApplyUser (options, kind, typed, result);
    CHR (hr);

    outName = typed;
    outPath = FilePath::Parse (typed);

    BAIL_OUT_IF (!options.disk.hasIndex, S_OK);

    //  A ProDOS disk, an index past the catalog, and an index for a name only
    //  one entry has are the same mistake, so all three get the same message.
    CBRFEx (isDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.Fail (options.disk.imagePath, "", m_context.ApplyPrefix (notAllowed.c_str())));

    {
        std::vector<Byte>  whole = sectors.ToVector();
        Dos33Volume        volume (whole);

        hr = volume.Enumerate (listing);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

    inRange = position < listing.entries.size();
    CBRFEx (inRange, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), result.Fail (options.disk.imagePath, "", m_context.ApplyPrefix (notAllowed.c_str())));

    if (!typed.empty())
    {
        matches = _stricmp (typed.c_str(), listing.entries[position].name.c_str()) == 0;
    }

    snprintf (note, sizeof (note), "does not match index %u", (unsigned) options.disk.index);
    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), result.Fail (options.disk.imagePath, options.disk.path, note));

    for (const FileEntry & entry : listing.entries)
    {
        sharing += (_stricmp (entry.name.c_str(), listing.entries[position].name.c_str()) == 0) ? 1 : 0;
    }

    shared = sharing > 1;
    CBRFEx (shared, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
            result.Fail (options.disk.imagePath, "", m_context.ApplyPrefix (notAllowed.c_str())));

    outName = listing.entries[position].name;
    outPath = FilePath::FromName (outName).WithLeafIndex (position);

Error:
    return hr;
}
