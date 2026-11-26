#include "Pch.h"

#include "DiskSectorCommands.h"
#include "DiskImageSession.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Utils.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands::RunSectorRead
//
//  THE OTHER HALF OF sectorwrite, and the reason it is worth having: get goes
//  through a catalog, so on a disk with no filesystem there was no way at all
//  to read back what had just been written. The listing said as much -- "its
//  files are in a layout this tool does not read" -- which was true and is no
//  longer.
//
//  A count is a parameter here and not on the write because a write knows its
//  own length and a read cannot: what usually records where a file ends is the
//  catalog, and these are the disks that have none.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSectorCommands::RunSectorRead (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         first        = 0;
    size_t                         total        = 0;
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    //  WHICH NUMBERING, OR NOTHING. The same sixteen sectors answer to two
    //  orders, and a command line that does not say which is how bytes once
    //  landed on the wrong sector -- so there is no default to fall back to.
    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, result.FailBadValue (
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibbleFormat::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibbleFormat::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibbleFormat::kTrackCount - 1,
                  NibbleFormat::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, result.FailBadValue (summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal sector count: %d\n"
                  "       The sector count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, result.FailBadValue (summary));

    first = (size_t) (options.disk.track * NibbleFormat::kSectorsPerTrack
                    + options.disk.sector);
    total = (size_t) (NibbleFormat::kTrackCount * NibbleFormat::kSectorsPerTrack);
    fits  = first + (size_t) options.disk.count <= total;

    if (!fits)
    {
        size_t  spare = total - first;

        snprintf (summary, sizeof (summary),
                  "Error: not enough sectors available\n"
                  "       Requested %d %s starting at track %d sector %d,\n"
                  "       but only %zu %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"),
                  options.disk.track, options.disk.sector,
                  spare,
                  Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, result.FailBadValue (summary));

    //  No filesystem needed, and none looked for. That is the whole point of
    //  the command: a disk built by sectorwrite may have no catalog at all.
    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    //  Each position maps through the STATED numbering, one at a time. Under
    //  --logical that is the identity into the DOS-ordered buffer; under
    //  --physical it is the interleave, so consecutive positions deliver the
    //  sectors in the order the drive presents them under consecutive
    //  address marks.
    for (int index = 0; index < options.disk.count; index++)
    {
        size_t  at = GetSectorRecordOffset (options.disk.numbering, first + (size_t) index);

        if (at + (size_t) NibbleFormat::kSectorByteSize > opened.sectors.size())
        {
            break;
        }

        payload.insert (payload.end(),
                        opened.sectors.begin() + (ptrdiff_t) at,
                        opened.sectors.begin() + (ptrdiff_t) (at + NibbleFormat::kSectorByteSize));
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_context.GetFileIo().WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from track %d %s sector %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.track, how, options.disk.sector,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, and it matters more here than
    //  anywhere: these bytes have no catalog and no length behind them, so a
    //  sector delivered as zeros looks exactly like a sector that holds zeros.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands::RunSectorWrite
//
//  A file from the host laid into an image at a track and a sector, in
//  whichever numbering the command line declared.
//
//  NO FILESYSTEM IS INVOLVED, WHICH IS THE POINT. A demo that boots its own
//  loader and reads fixed tracks has no catalog to make an entry in and no
//  allocator to ask for space, so `put` cannot express it at all. This writes
//  the bytes given, where it is told, and nothing else.
//
//  THE NUMBERING IS DECLARED, NEVER DEFAULTED. The same sixteen sectors
//  answer to two orders: --logical is what catalogs, RWTS callers and every
//  DOS-era sector editor speak, the identity into a DOS-ordered image; and
//  --physical is the address-field order a boot loader asking the drive ROM
//  sees, the interleave. This command once applied the interleave silently
//  under the belief the image was physical-ordered -- `--sector 1` landed on
//  logical sector 7, and sectorread's matching belief read it back perfectly
//  -- which is why an unstated numbering is refused rather than guessed. The
//  skew case in DirectBootTests is what settled the orientation against
//  DOS's own table at $084D.
//
//  It runs on past the end of a track into the next one, because a payload
//  longer than 4 KB is ordinary and splitting the call per track would put the
//  wrap arithmetic back in the caller's hands. Under --physical the run-on
//  advances by address mark, so page N of the payload sits under mark N --
//  exactly what a loader that files sectors by address mark wants.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSectorCommands::RunSectorWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, m_context.ReportMissingParameter ("<file>", result));

    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, result.FailBadValue (
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibbleFormat::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibbleFormat::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibbleFormat::kTrackCount - 1,
                  NibbleFormat::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, result.FailBadValue (summary));

    hr = m_context.GetFileIo().ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A disk with no filesystem is the ordinary case here rather than a
    //  refusal: a demo that boots its own loader has no catalog, and that
    //  is exactly the disk this command exists to write.
    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    edited = opened.sectors;

    //  Whole sectors, because that is the unit the drive reads. A payload that
    //  does not fill its last one is padded with what was already there rather
    //  than with a value this command invented.
    needed = (payload.size() + (size_t) NibbleFormat::kSectorByteSize - 1)
           / (size_t) NibbleFormat::kSectorByteSize;

    {
        size_t  first = (size_t) (options.disk.track * NibbleFormat::kSectorsPerTrack
                                + options.disk.sector);
        size_t  total = (size_t) (NibbleFormat::kTrackCount * NibbleFormat::kSectorsPerTrack);

        fits = first + needed <= total;

        if (!fits)
        {
            size_t       spare  = total - first;
            snprintf (summary, sizeof (summary),
                      "Error: not enough sectors available\n"
                      "       Writing %zu bytes starting at track %d sector %d requires\n"
                      "       %zu %s, but only %zu %s %s on the disk.\n",
                      payload.size(), options.disk.track, options.disk.sector,
                      needed,
                      Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"),
                      spare,
                      Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                      (spare == 1) ? "remains" : "remain");
        }

        CBRFEx (fits, E_INVALIDARG, result.FailBadValue (summary));

        for (size_t index = 0; index < needed; index++)
        {
            size_t  at    = GetSectorRecordOffset (options.disk.numbering, first + index);
            size_t  from  = index * (size_t) NibbleFormat::kSectorByteSize;
            size_t  count = std::min ((size_t) NibbleFormat::kSectorByteSize,
                                      payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_context.GetSession().SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at track %d %s sector %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.track, how, options.disk.sector,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands::GetSectorRecordOffset
//
//  Positions advance linearly -- sector, then track -- and each one maps
//  through the stated numbering on its own. Under --logical that is the
//  identity into the DOS-ordered buffer; under --physical it is the
//  interleave, answered by the layer that owns the skew rather than by a
//  second copy of the sixteen numbers here.
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskSectorCommands::GetSectorRecordOffset (
    CommandLineOptions::DiskOptions::Numbering  numbering,
    size_t                                      running)
{
    size_t  track    = running / (size_t) NibbleFormat::kSectorsPerTrack;
    size_t  position = running % (size_t) NibbleFormat::kSectorsPerTrack;
    bool    physical = numbering == CommandLineOptions::DiskOptions::Numbering::Physical;
    size_t  record   = running;



    if (physical)
    {
        record = track * (size_t) NibbleFormat::kSectorsPerTrack
               + (size_t) SectorInterleave::GetDosFileIndexForPhysicalSector ((int) position);
    }

    return record * (size_t) NibbleFormat::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands::RunBlockRead
//
//  The 512-byte view: a ProDOS block is two sector records spread across its
//  track by the ProDOS interleave, and the map is ProDosSkeleton's -- the
//  same single copy the ProDOS reader, writer and builder address blocks
//  through.
//
//  A BLOCK NUMBER NEEDS NO --logical OR --physical, because blocks have only
//  one order: the block map is defined over the volume, so the question the
//  sector commands must ask has one answer here.
//
//  And it works over any container, not only .po. The session normalizes
//  every image into the same DOS-ordered buffer, so a block is a lens over
//  that buffer -- which is how a ProDOS volume shipped inside a .dsk reads
//  naturally.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSectorCommands::RunBlockRead (const CommandLineOptions & options,
                                      DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes = (size_t) NibbleFormat::kSectorByteSize;



    HRESULT                        hr           = S_OK;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    int                            blocks       = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    //  No filesystem needed, and none looked for: a block is geometry, not
    //  a volume, and blocks 0-1 of a disk with no filesystem are still worth
    //  looking at.
    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    blocks = ProDosSkeleton::GetBlockCount (opened.sectors.size());
    onDisk = options.disk.block >= 0 && options.disk.block < blocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, blocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, result.FailBadValue (summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal block count: %d\n"
                  "       The block count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, result.FailBadValue (summary));

    fits = options.disk.block + options.disk.count <= blocks;

    if (!fits)
    {
        int  spare = blocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Requested %d %s starting at block %d,\n"
                  "       but only %d %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"),
                  options.disk.block,
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, result.FailBadValue (summary));

    for (int index = 0; index < options.disk.count; index++)
    {
        int  block = options.disk.block + index;

        for (int half = 0; half < 2; half++)
        {
            size_t  at = ProDosSkeleton::GetBlockByteOffset (opened.sectors.size(), block, (size_t) half * kHalfBytes);

            if (at + kHalfBytes > opened.sectors.size())
            {
                break;
            }

            payload.insert (payload.end(),
                            opened.sectors.begin() + (ptrdiff_t) at,
                            opened.sectors.begin() + (ptrdiff_t) (at + kHalfBytes));
        }
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_context.GetFileIo().WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from block %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.block,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, exactly as the sector read
    //  reports it: a block delivered as zeros looks like a block that holds
    //  zeros, and nothing behind these bytes records the difference.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands::RunBlockWrite
//
//  The write half of the 512-byte view, through the same single block map.
//
//  Whole blocks, the way the sector write works in whole sectors: a payload
//  that does not fill its last block leaves the rest of that block as it
//  was, rather than padded with a value this command invented. It runs on
//  into later blocks for the same reason the sector write runs on into
//  later tracks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSectorCommands::RunBlockWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes  = (size_t) NibbleFormat::kSectorByteSize;
    constexpr size_t  kBlockBytes = (size_t) ProDosSkeleton::kBlockByteSize;



    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           onDisk       = false;
    bool                           fits         = false;
    int                            blocks       = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, m_context.ReportMissingParameter ("<file>", result));

    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    blocks = ProDosSkeleton::GetBlockCount (opened.sectors.size());
    onDisk = options.disk.block >= 0 && options.disk.block < blocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, blocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, result.FailBadValue (summary));

    hr = m_context.GetFileIo().ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    edited = opened.sectors;

    needed = (payload.size() + kBlockBytes - 1) / kBlockBytes;

    fits = (size_t) options.disk.block + needed <= (size_t) blocks;

    if (!fits)
    {
        int  spare = blocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Writing %zu bytes starting at block %d requires\n"
                  "       %zu %s, but only %d %s %s on the disk.\n",
                  payload.size(), options.disk.block,
                  needed,
                  Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"),
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, result.FailBadValue (summary));

    for (size_t index = 0; index < needed; index++)
    {
        int  block = options.disk.block + (int) index;

        for (int half = 0; half < 2; half++)
        {
            size_t  from  = index * kBlockBytes + (size_t) half * kHalfBytes;
            size_t  at    = ProDosSkeleton::GetBlockByteOffset (edited.size(), block, (size_t) half * kHalfBytes);
            size_t  count = 0;

            if (from >= payload.size())
            {
                break;
            }

            count = std::min (kHalfBytes, payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_context.GetSession().SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at block %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.block,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}
