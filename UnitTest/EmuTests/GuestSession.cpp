#include "Pch.h"
#include "../EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Devices/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::ReadFileOrEmpty
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::ReadFileOrEmpty (const std::filesystem::path & full)
{
    std::error_code    ec;
    std::vector<Byte>  bytes;
    bool               present = std::filesystem::exists (full, ec);



    if (present)
    {
        std::ifstream  file (full, std::ios::binary);
        bool           opened = file.good();

        if (opened)
        {
            bytes.assign ((std::istreambuf_iterator<char> (file)),
                          std::istreambuf_iterator<char> ());
        }
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::FindStockImage
//
//  The two places a stock image can be, tried in order: `repoPath` under the
//  working directory and each of its parents, then the emulator's download
//  cache under `cacheName`. READ-ONLY: nothing here downloads, copies, or
//  creates anything, so a run leaves no trace of having looked.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::FindStockImage (
    const char     * repoPath,
    const wchar_t  * cacheName)
{
    std::error_code        ec;
    std::filesystem::path  cursor    = std::filesystem::current_path (ec);
    std::vector<Byte>      bytes;
    wchar_t              * cacheRoot = nullptr;
    size_t                 len       = 0;
    bool                   walking   = !ec;
    bool                   hasCache  = false;
    int                    level     = 0;



    for (level = 0; walking && bytes.empty() && level < kMaxAncestorWalk; level++)
    {
        bytes = ReadFileOrEmpty (cursor / repoPath);

        if (bytes.empty())
        {
            bool  atRoot = !cursor.has_parent_path() || cursor == cursor.parent_path();

            if (atRoot)
            {
                walking = false;
            }
            else
            {
                cursor = cursor.parent_path();
            }
        }
    }

    if (bytes.empty())
    {
        hasCache = _wdupenv_s (&cacheRoot, &len, L"LOCALAPPDATA") == 0 && cacheRoot != nullptr;
    }

    if (hasCache)
    {
        bytes = ReadFileOrEmpty (std::filesystem::path (cacheRoot) /
                                 L"Casso" / L"Disks" / cacheName);
        free (cacheRoot);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::RequireDos33Master
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::RequireDos33Master()
{
    std::vector<Byte>  bytes = FindStockImage (kpszMasterRepoPath, kpszMasterCacheName);



    Assert::IsFalse (bytes.empty(),
        L"the stock DOS 3.3 master is REQUIRED by this gate and was not found, either as "
        L"Disks/Apple/dos33-master.dsk in a parent of the working directory or as "
        L"%LOCALAPPDATA%\\Casso\\Disks\\DOS 3.3 System Master.dsk. To get it, pick the "
        L"DOS 3.3 row in Casso's Boot Disk or Insert Disk picker once, which downloads it "
        L"into that cache, or place a copy at the repo path, which is gitignored. This "
        L"case fails rather than skipping: a guest-visible gate that never started a "
        L"guest has checked nothing, and a skip is indistinguishable in the output from "
        L"a case that ran");

    Assert::AreEqual (kImageBytes, bytes.size(),
        L"and it must be a whole 5.25-inch sector image");

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::RequireProDosUsersDisk
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::RequireProDosUsersDisk()
{
    std::vector<Byte>  bytes = FindStockImage (kpszProDosRepoPath, kpszProDosCacheName);



    Assert::IsFalse (bytes.empty(),
        L"the stock ProDOS Users Disk is REQUIRED by this gate and was not found, either "
        L"as Disks/Apple/prodos-users.dsk in a parent of the working directory or as "
        L"%LOCALAPPDATA%\\Casso\\Disks\\ProDOS Users Disk.dsk. To get it, pick the ProDOS "
        L"row in Casso's Boot Disk or Insert Disk picker once, which downloads it into "
        L"that cache, or place a copy at the repo path, which is gitignored. This case "
        L"fails rather than skipping: a guest-visible gate that never started a guest has "
        L"checked nothing, and a skip is indistinguishable in the output from a case that "
        L"ran");

    Assert::AreEqual (kImageBytes, bytes.size(),
        L"and it must be a whole 5.25-inch sector image");

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::TrimTrailingBlanks
//
////////////////////////////////////////////////////////////////////////////////

std::string GuestSession::TrimTrailingBlanks (const std::string & row)
{
    std::string  trimmed = row;
    size_t       end     = trimmed.find_last_not_of (' ');



    if (end == std::string::npos)
    {
        return std::string();
    }

    trimmed.erase (end + 1);

    return trimmed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::DecodeThroughTheDrive
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::DecodeThroughTheDrive (const std::vector<Byte> & bytes)
{
    DiskImage           image;
    SectorDecodeReport  report;



    AssertSucceeded (NibblizationLayer::NibblizeDsk (bytes, image),
        L"the container must nibblize before the drive can report what it reads");

    return DecodeThroughTheDrive (image, report);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::DecodeThroughTheDrive
//
//  The image's sectors as the DRIVE reads them, rather than as the container
//  stores them, with the report of what decoded.
//
//  Going through the nibble layer is the point: a test that read the container
//  directly would agree with whatever wrote it, and the question here is what a
//  6502 booting this disk actually gets.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::DecodeThroughTheDrive (const DiskImage     & image,
                                                       SectorDecodeReport  & outReport)
{
    std::vector<Byte>  sectors;



    //  The REPORTING overload, and not for the report alone. Its twin fails on
    //  any data loss, and a copy-protected disk loses data by construction --
    //  which would turn "this WOZ cannot be pre-checked" into "this WOZ is
    //  broken" at every game gate on the branch.
    AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, sectors, outReport),
        L"and the drive must be able to walk its tracks at all");

    return sectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AssertTheDrivePresentsWhatWasMounted
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::AssertTheDrivePresentsWhatWasMounted (const DiskImage          & image,
                                                         const std::vector<Byte>  & mounted,
                                                         const wchar_t            * what)
{
    SectorDecodeReport  report;
    std::vector<Byte>   seen = DecodeThroughTheDrive (image, report);
    size_t              at   = 0;



    Assert::AreEqual (mounted.size(), seen.size(), std::format (
        L"{}: the drive presents {} bytes where {} were mounted",
        what, seen.size(), mounted.size()).c_str());

    for (at = 0; at < seen.size(); at++)
    {
        if (seen[at] != mounted[at])
        {
            break;
        }
    }

    Assert::IsTrue (at == seen.size(), std::format (
        L"{}: the drive does not read back what was mounted. First difference at offset "
        L"{} -- track {}, sector {} -- where ${:02X} was mounted and ${:02X} comes back. "
        L"A sector out of step still carries a valid header, so the ROM would read it, "
        L"believe it, and jump into it",
        what,
        at,
        at / ((size_t) NibblizationLayer::kSectorByteSize * NibblizationLayer::kSectorsPerTrack),
        (at / (size_t) NibblizationLayer::kSectorByteSize) % NibblizationLayer::kSectorsPerTrack,
        (unsigned) (at < mounted.size() ? mounted[at] : 0),
        (unsigned) (at < seen.size()    ? seen[at]    : 0)).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AssertTheDriveCanReadTheBootSector
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::AssertTheDriveCanReadTheBootSector (const DiskImage  & image,
                                                       const wchar_t    * what)
{
    SectorDecodeReport  report;
    std::vector<Byte>   sectors = DecodeThroughTheDrive (image, report);
    bool                blank   = true;
    size_t              i       = 0;



    Assert::IsTrue (report.IsSectorRecovered (kBootTrack, kBootSector), std::format (
        L"{}: the drive cannot recover track 0's first sector, which is the one the "
        L"controller ROM reads into $0800 and jumps into. Booting this would put a 6502 "
        L"into data and write a look-back per illegal opcode until something killed it",
        what).c_str());

    for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize && blank; i++)
    {
        blank = sectors[i] == 0;
    }

    Assert::IsFalse (blank, std::format (
        L"{}: track 0's first sector decoded and is entirely zero, so the ROM would hand "
        L"over to a page of BRKs",
        what).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AssertTheDriveHoldsWrittenTracks
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::AssertTheDriveHoldsWrittenTracks (const DiskImage  & image,
                                                     int                leastTracks,
                                                     const wchar_t    * what)
{
    SectorDecodeReport  report;
    int                 written = 0;
    int                 slot    = 0;



    DecodeThroughTheDrive (image, report);

    for (slot = 0; slot < image.GetTrackCount(); slot++)
    {
        if (image.GetTrackBitCount (slot) > 0)
        {
            written++;
        }
    }

    Assert::IsTrue (written >= leastTracks, std::format (
        L"{}: the drive is holding only {} written tracks and this gate is about to "
        L"require the head to visit {}. Booting it would spend the whole cycle budget "
        L"with a 6502 in data, writing a look-back per illegal opcode",
        what, written, leastTracks).c_str());

    Assert::IsTrue (TrackDecodeOutcome::Unformatted != report.GetOutcome (kBootTrack),
        std::format (
            L"{}: track 0 carries no address field anywhere in a revolution, so the "
            L"controller ROM has nothing to recalibrate onto and would hand a page of "
            L"whatever is in memory to the processor",
            what).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::IsAtBarePrompt
//
////////////////////////////////////////////////////////////////////////////////

bool GuestSession::IsAtBarePrompt (EmulatorCore & core)
{
    std::vector<std::string>  rows  = TextScreenScraper::Scrape (core);
    std::string               row;
    size_t                    index = 0;



    for (index = rows.size(); index > 0; index--)
    {
        row = TrimTrailingBlanks (rows[index - 1]);

        if (!row.empty())
        {
            return row.size() <= kPromptRowLength && row[0] == kPromptGlyph;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::CollectRows
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::CollectRows (EmulatorCore & core, std::vector<std::string> & outRows)
{
    std::vector<std::string>  rows = TextScreenScraper::Scrape (core);



    for (const std::string & row : rows)
    {
        std::string  trimmed = TrimTrailingBlanks (row);

        if (!trimmed.empty())
        {
            outRows.push_back (trimmed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::TryPageToPrompt
//
////////////////////////////////////////////////////////////////////////////////

bool GuestSession::TryPageToPrompt (EmulatorCore & core, std::vector<std::string> & outRows)
{
    bool  atPrompt = false;
    int   page     = 0;



    for (page = 0; page < kMaxPages && !atPrompt; page++)
    {
        CollectRows (core, outRows);

        atPrompt = IsAtBarePrompt (core);

        if (!atPrompt)
        {
            KeystrokeInjector::InjectLine (core, "", kLineCycles);
        }
    }

    return atPrompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::TypeAndCollect
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> GuestSession::TypeAndCollect (EmulatorCore & core, const std::string & line)
{
    std::vector<std::string>  rows;
    bool                      atPrompt = false;



    KeystrokeInjector::InjectLine (core, line, kLineCycles);

    atPrompt = TryPageToPrompt (core, rows);

    Assert::IsTrue (atPrompt,
        L"the guest must come back to its prompt after the typed line");

    Assert::IsTrue (rows.size() > 0,
        L"and must have printed something, or the assertions below compare nothing");

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AnyRowIs
//
////////////////////////////////////////////////////////////////////////////////

bool GuestSession::AnyRowIs (const std::vector<std::string> & rows, const std::string & wanted)
{
    return std::find (rows.begin(), rows.end(), wanted) != rows.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AnyRowContains
//
////////////////////////////////////////////////////////////////////////////////

bool GuestSession::AnyRowContains (const std::vector<std::string> & rows, const std::string & needle)
{
    for (const std::string & row : rows)
    {
        if (row.find (needle) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::AssertTheOnlyRowsMentioning
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::AssertTheOnlyRowsMentioning (
    const std::vector<std::string>  & rows,
    const std::string               & needle,
    const std::string               & expected)
{
    size_t  matching = 0;



    for (const std::string & row : rows)
    {
        if (row.find (needle) == std::string::npos)
        {
            continue;
        }

        Assert::AreEqual (expected, row,
            L"every row the guest printed about this file must be the row expected");

        matching++;
    }

    Assert::IsTrue (matching > 0,
        L"and the guest must have printed at least one -- a listing that never mentions "
        L"the file satisfies a per-row rule vacuously");
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::Mount
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::Mount (HeadlessHost             & host,
                          EmulatorCore             & core,
                          const std::vector<Byte>  & bytes)
{
    DiskImage *  image = nullptr;



    AssertSucceeded (host.BuildApple2eWithDisk2 (core), L"BuildApple2eWithDisk2 must succeed");

    core.PowerCycle();

    AssertSucceeded (core.diskStore->MountFromBytes (kSlot6, kDrive1, "gate.dsk",
                                                     DiskFormat::Dsk, bytes),
        L"MountFromBytes must succeed");

    image = core.diskStore->GetImage (kSlot6, kDrive1);
    Assert::IsNotNull (image, L"the mounted image must be present");
    core.diskController->SetExternalDisk (kDrive1, image);

    //  Between mounting and starting the processor, and nowhere else: this is
    //  the last moment at which a hopeless image costs milliseconds instead of
    //  a runaway trace. Every caller here hands over a sector buffer, so every
    //  caller gets the strongest of the three questions asked for it.
    AssertTheDrivePresentsWhatWasMounted (*image, bytes, L"the image this gate mounts");
    AssertTheDriveCanReadTheBootSector   (*image,        L"the image this gate mounts");

    core.bus->WriteByte (kIntCxRomOff, 0);
    core.cpu->SetPC (kBootRomEntry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::MountAndBoot
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::MountAndBoot (HeadlessHost             & host,
                                 EmulatorCore             & core,
                                 const std::vector<Byte>  & bytes)
{
    Mount (host, core, bytes);

    MachineIdle::RunUntilIdle (core, kBootCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::BootToPrompt
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::BootToPrompt (HeadlessHost             & host,
                                 EmulatorCore             & core,
                                 const std::vector<Byte>  & bytes)
{
    std::vector<std::string>  rows;
    bool                      atPrompt = false;



    MountAndBoot (host, core, bytes);

    atPrompt = TryPageToPrompt (core, rows);

    Assert::IsTrue (atPrompt,
        L"the image must boot its operating system through to a BASIC prompt");

    Assert::IsFalse (AnyRowContains (rows, "I/O ERROR"),
        L"and must not have hit a read error on the way");
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession::GuestBytesAt
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::GuestBytesAt (EmulatorCore & core, Word address, size_t count)
{
    std::vector<Byte>  bytes (count, 0);
    size_t             i = 0;



    for (i = 0; i < count; i++)
    {
        bytes[i] = core.bus->ReadByte ((Word) (address + i));
    }

    return bytes;
}
