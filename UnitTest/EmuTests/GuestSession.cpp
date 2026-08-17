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
//  GuestSession::RequireDos33Master
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> GuestSession::RequireDos33Master()
{
    std::error_code        ec;
    std::filesystem::path  cursor    = std::filesystem::current_path (ec);
    std::vector<Byte>      bytes;
    wchar_t              * cacheRoot = nullptr;
    size_t                 len       = 0;
    bool                   walking   = !ec;
    bool                   hasCache  = false;
    int                    level     = 0;



    for (level = 0; walking && bytes.empty() && level < 10; level++)
    {
        bytes = ReadFileOrEmpty (cursor / kpszMasterRepoPath);

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
                                 L"Casso" / L"Disks" / kpszMasterCacheName);
        free (cacheRoot);
    }

    Assert::IsFalse (bytes.empty(),
        L"the stock DOS 3.3 master is REQUIRED by this gate and was not found, either as "
        L"Disks/Apple/dos33-master.dsk in a parent of the working directory or as "
        L"%LOCALAPPDATA%\\Casso\\Disks\\DOS 3.3 System Master.dsk. This case fails rather "
        L"than skipping: a guest-visible gate that never started a guest has checked "
        L"nothing, and a skip is indistinguishable in the output from a case that ran");

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
//  GuestSession::MountAndBoot
//
////////////////////////////////////////////////////////////////////////////////

void GuestSession::MountAndBoot (HeadlessHost             & host,
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

    core.bus->WriteByte (kIntCxRomOff, 0);
    core.cpu->SetPC (kBootRomEntry);

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
