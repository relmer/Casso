#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::SeedRealDisk
//
//  Seeds the fake platform with a real disk, so the command path is
//  exercised against material nobody here authored.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::SeedRealDisk (
    FakeDiskFileIo  & io,
    const char      * fixture,
    const char      * asPath)
{
    FixtureProvider  fixtures;
    vector<Byte>     bytes;



    AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

    io.files[asPath]  = bytes;
    io.stamps[asPath] = FileStamp { bytes.size(), 100 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeOptions
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakeOptions (
    CommandLineOptions::DiskOptions::Command    command,
    const char                                * image)
{
    CommandLineOptions  options;



    options.subcommand     = CommandLineOptions::Subcommand::Disk;
    options.disk.command      = command;
    options.disk.imagePath = image;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::AssertEveryLineFitsEightyColumns
//
//  A LISTING HAS TO FIT AN 80-COLUMN TERMINAL, and that is the constraint
//  that made the ProDOS columns safe to print unconditionally rather than
//  behind a flag. A row that wraps is a row that reads as two entries.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::AssertEveryLineFitsEightyColumns (
    const std::string  & text)
{
    size_t  lineStart = 0;
    size_t  lineEnd   = 0;



    while (lineStart < text.size())
    {
        std::string  line;

        lineEnd = text.find ('\n', lineStart);
        line    = (lineEnd == std::string::npos)
                      ? text.substr (lineStart)
                      : text.substr (lineStart, lineEnd - lineStart);

        Assert::IsTrue (line.size() <= 80,
            (std::wstring (L"line runs past column 80: ")
                 + TextEncoding::NarrowToWide (line)).c_str());

        if (lineEnd == std::string::npos)
        {
            break;
        }

        lineStart = lineEnd + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::SeedProDosDisk
//
//  A ProDOS image with room to make directories on.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::SeedProDosDisk (
    FakeDiskFileIo  & io)
{
    SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::ExpectedMakeDumpPayload
//
//  The independently extracted copy, minus the four-byte DOS 3.3 header the
//  reader consumes. Comparing against these bytes is strictly stronger than
//  comparing a length: a length check passes under any corruption that
//  happens to preserve size, and the oracle is already sitting there.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> DiskRunnerFixtures::ExpectedMakeDumpPayload()
{
    FixtureProvider  fixtures;
    vector<Byte>     stored;



    AssertSucceeded (fixtures.OpenFixture ("Merlin/MAKE DUMP", stored));
    Assert::AreEqual (size_t (593), stored.size(), L"the stored file carries its header");

    return vector<Byte> (stored.begin() + 4, stored.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::AssertIsMakeDumpPayload
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::AssertIsMakeDumpPayload (
    const vector<Byte>  & actual)
{
    vector<Byte>  expected = ExpectedMakeDumpPayload();
    size_t        i        = 0;



    // 589 is the length the header declares. Naming it here makes the
    // failure legible: 618 would mean 29 line-feed bytes were expanded on
    // the way out, which is the text-mode corruption this file's payload is
    // uniquely suited to expose.
    Assert::AreEqual (size_t (589), expected.size());
    Assert::AreEqual (expected.size(), actual.size());

    for (i = 0; i < expected.size(); i++)
    {
        if (actual[i] != expected[i])
        {
            Assert::Fail (L"payload differs from the independently extracted copy");
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::SeedBlankImage
//
//  A blank .dsk to write into, with no filesystem on it at all.
//
//  THE STAMP MATTERS AS MUCH AS THE BYTES. A commit compares the file it
//  is about to replace against the one that was read, and refuses when it
//  cannot: an image seeded with contents and no stamp is one the runner
//  will decline to write, which is correct of it and easy to mistake for
//  a bug in the command under test.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::SeedBlankImage (
    FakeDiskFileIo  & io,
    const char      * path)
{
    FileStamp  stamp;



    io.files[path] = vector<Byte> ((size_t) NibbleFormat::kImageByteSize, (Byte) 0x00);

    stamp.sizeBytes   = (uint64_t) io.files[path].size();
    stamp.modifiedUnix = 1;

    io.stamps[path] = stamp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeSectorRead
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakeSectorRead (
    const char                                  * image,
    int                                           track,
    int                                           sector,
    int                                           count,
    CommandLineOptions::DiskOptions::Numbering    numbering)
{
    CommandLineOptions  options;



    options.subcommand        = CommandLineOptions::Subcommand::Disk;
    options.disk.command      = CommandLineOptions::DiskOptions::Command::SectorRead;
    options.disk.commandWord  = "sectorread";
    options.disk.imagePath    = image;
    options.disk.track        = track;
    options.disk.sector       = sector;
    options.disk.count        = count;
    options.disk.numbering    = numbering;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeDirectBoot
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakeDirectBoot (
    const char  * image,
    const char  * payload)
{
    CommandLineOptions  options = MakeCreate (image);



    options.disk.directBootFile = payload;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeCreate
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakeCreate (
    const char  * path)
{
    CommandLineOptions  options;



    options.subcommand     = CommandLineOptions::Subcommand::Disk;
    options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
    options.disk.commandWord  = "create";
    options.disk.imagePath = path;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::OriginalImageBytes
//
//  The image exactly as it was seeded, kept for comparison. Read from the
//  fixture rather than from the fake, so a commit that corrupted the fake's
//  copy cannot also corrupt the oracle.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> DiskRunnerFixtures::OriginalImageBytes()
{
    FixtureProvider  fixtures;
    vector<Byte>     bytes;



    AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", bytes));
    Assert::IsTrue (bytes.size() > 0, L"the oracle must actually have been read");

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeBlankDos33Image
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> DiskRunnerFixtures::MakeBlankDos33Image()
{
    vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);



    AssertSucceeded (Dos33Skeleton::Write (buffer, kBlankVolumeNumber));

    return buffer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::SeedFile
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::SeedFile (
    FakeDiskFileIo      & io,
    const char          * path,
    const vector<Byte>  & bytes)
{
    io.files[path]  = bytes;
    io.stamps[path] = FileStamp { bytes.size(), 100 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakePayload
//
//  A payload nothing else on the disk could be mistaken for, so a read-back
//  comparison fails loudly rather than matching some neighboring sector.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> DiskRunnerFixtures::MakePayload (
    size_t  count)
{
    vector<Byte>  bytes (count, 0);
    size_t        i     = 0;



    for (i = 0; i < count; i++)
    {
        bytes[i] = (Byte) ((i * 7 + 0x21) & 0xFF);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakePutOptions
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakePutOptions (
    const char  * image,
    const char  * hostFile,
    const char  * asName)
{
    CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Put, image);



    options.disk.hostFile = hostFile;

    if (asName != nullptr)
    {
        options.disk.path = asName;
    }

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::GetFromCommittedImage
//
//  Reads a file back through a FRESH runner over the committed image, which
//  is the only way to prove the bytes went to the disk rather than merely
//  through the runner that put them there.
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandResult DiskRunnerFixtures::GetFromCommittedImage (
    FakeDiskFileIo                             & io,
    const char                                 * image,
    const char                                 * path,
    CommandLineOptions::DiskOptions::Encoding    encoding)
{
    DiskCommandRunner   reader (io);
    CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get, image);



    options.disk.path     = path;
    options.disk.encoding = encoding;

    return reader.Run (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::ListCommittedImage
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskRunnerFixtures::ListCommittedImage (
    FakeDiskFileIo  & io,
    const char      * image)
{
    DiskCommandRunner  reader (io);



    return reader.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List, image)).output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::AssertImageMatches
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::AssertImageMatches (
    FakeDiskFileIo      & io,
    const char          * path,
    const vector<Byte>  & expected)
{
    size_t  i = 0;



    Assert::AreEqual (size_t (1), io.files.count (path), L"the image must still be there");
    Assert::AreEqual (expected.size(), io.files[path].size(), L"and be the size it was");

    for (i = 0; i < expected.size(); i++)
    {
        if (io.files[path][i] != expected[i])
        {
            Assert::Fail (L"the image differs from what it was before the refused write");
        }
    }

    Assert::IsTrue (io.HasNoTemporaryFiles(), L"and no temporary may be left beside it");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::AssertNamesNoPlatformCode
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::AssertNamesNoPlatformCode (
    const std::string  & diagnostics)
{
    // FR-014's demand is that a refusal reads as a reason. A hexadecimal
    // HRESULT is the specific failure mode it forbids, and it is the shape
    // a message picks up the moment somebody formats `hr` into it.
    Assert::IsTrue (diagnostics.find ("0x") == std::string::npos,
        L"a refusal must not carry a raw platform code");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::LockFirstCatalogEntry
//
//  Sets the lock bit on the first catalog entry, which is where a file
//  placed onto a freshly formatted volume lands.
//
////////////////////////////////////////////////////////////////////////////////

void DiskRunnerFixtures::LockFirstCatalogEntry (
    vector<Byte>  & buffer)
{
    size_t  at = Dos33Skeleton::GetSectorOffset (kVtocTrack, kCatalogFirstSector)
               + kEntryBase + kEntOffType;



    buffer[at] = (Byte) (buffer[at] | kLockedBit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures::MakeBinaryPutOptions
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskRunnerFixtures::MakeBinaryPutOptions (
    const char  * image)
{
    CommandLineOptions  options = MakePutOptions (image, kHostFile, "PROG");



    options.disk.loadAddress    = kLoadAddress;
    options.disk.hasLoadAddress = true;

    return options;
}





