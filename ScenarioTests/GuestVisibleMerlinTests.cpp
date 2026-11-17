#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleMerlinTests
//
//  A Merlin source copied out of Merlin Pro's ProDOS disk as text and put
//  back under another name, loaded by Merlin itself on an enhanced //e
//  (SC-048). The byte-for-byte round trip is FileTypeFixtureTests'; this is
//  the program that wrote the format reading the file Casso made.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleMerlinTests)
{
public:

    //  Merlin's Load source takes the file and opens its editor, and the
    //  editor lists the file from its first line to its last.
    TEST_METHOD (MerlinLoadsASourcePutBackOntoItsProDosDisk)
    {
        PutBack      putBack = MakeDiskWithPutBackSource ("Disks/Merlin-proProdos2.33-a.dsk", VolumeKind::ProDos, "SOURCE/", "PI.NAMES.S");
        TestMachine  machine ("Apple2eEnhanced", TestMachine::Video::Timed);



        GuestSession::MountAndBoot (machine, putBack.disk);

        TypeAndWait (machine, "",               "Load source");
        TypeAndWait (machine, "L",              "Load:");
        TypeAndWait (machine, "SOURCE/CASSO\r", "Editor");
        TypeAndWait (machine, "L\r",            putBack.firstWord.c_str());
        TypeAndWait (machine, "",               putBack.lastWord.c_str());
    }


    TEST_METHOD (MerlinLoadsASourcePutBackOntoItsDos33Disk)
    {
        PutBack      putBack = MakeDiskWithPutBackSource ("Disks/Merlin-proDos2.23.dsk", VolumeKind::Dos33, "", "LABELS.S");
        TestMachine  machine ("Apple2eEnhanced", TestMachine::Video::Timed);



        GuestSession::MountAndBoot (machine, putBack.disk);

        TypeAndWait (machine, "",        "Load source");
        TypeAndWait (machine, "L",       "Load:");
        TypeAndWait (machine, "CASSO\r", "Editor");
        TypeAndWait (machine, "L\r",     putBack.firstWord.c_str());
        TypeAndWait (machine, "",        putBack.lastWord.c_str());
    }

private:

    //  The disk, and the first word of the source's first line and the last
    //  word of its last line.
    struct PutBack
    {
        std::vector<Byte>  disk;
        std::string        firstWord;
        std::string        lastWord;
    };

    //  The pause after each key, how often the screen is read, and the
    //  cycles run before a missing prompt fails the test.
    static constexpr uint64_t  kKeyGapCycles  = 60'000;
    static constexpr uint64_t  kPollCycles    = 250'000;
    static constexpr uint64_t  kCeilingCycles = 60'000'000;

    //  The keys typed, then the machine run until a row contains `prompt`.
    static void TypeAndWait (MachineHost & machine, const char * keys, const char * prompt)
    {
        uint64_t     spent = 0;
        bool         found = false;
        std::string  rows;



        for (const char * c = keys; *c != '\0'; c++)
        {
            KeystrokeInjector::InjectKey (machine, static_cast<Byte> (*c));
            machine.RunCycles (kKeyGapCycles);
        }

        while (!found && spent < kCeilingCycles)
        {
            spent += machine.RunCycles (kPollCycles);
            found  = GuestSession::AnyRowContains (TextScreenScraper::Scrape (machine), prompt);
        }

        for (const std::string & row : TextScreenScraper::Scrape (machine))
        {
            rows += GuestSession::TrimTrailingBlanks (row) + " | ";
        }

        Assert::IsTrue (found, TextEncoding::NarrowToWide (std::format ("no row contains \"{}\" after \"{}\": {}", prompt, keys, rows)).c_str());
    }

    //  A Merlin disk with `folder` + `source` copied out as Merlin text and
    //  put back in the folder as CASSO.S, as the bytes of a .dsk.
    static PutBack MakeDiskWithPutBackSource (const char * fixture, VolumeKind kind, const std::string & folder, const std::string & source)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::vector<Byte>         written;
        std::vector<Byte>         saved;
        std::vector<Byte>         text;
        SectorDecodeReport        report;
        VolumeListing             listing;
        FilePayload               payload;
        FilePayload               imported;
        DecodedFile               decoded;
        FileFormat                format  = FileFormat::None;
        std::wstring              error;
        std::string               reason;
        std::string               exported;
        const FileEntry         * entry   = nullptr;
        std::unique_ptr<IVolume>  volume;
        PutBack                   putBack;



        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, "merlin.dsk", sectors, report));

        volume = VolumeImage::CreateVolume (kind, sectors);
        Assert::IsNotNull (volume.get());

        if (folder.empty())
        {
            AssertSucceeded (volume->Enumerate (listing));
        }
        else
        {
            AssertSucceeded (volume->EnumerateDirectory (FilePath::Parse (folder.substr (0, folder.size() - 1)), listing));
        }

        for (const FileEntry & candidate : listing.entries)
        {
            if (candidate.name == source)
            {
                entry = &candidate;
            }
        }

        Assert::IsNotNull (entry, TextEncoding::NarrowToWide ("the disk has " + folder + source).c_str());
        AssertSucceeded   (volume->Read (FilePath::Parse (folder + source), payload));

        format = FileFormatTable::Identify (*entry, kind, payload.bytes);
        Assert::IsTrue (format == FileFormat::Merlin, TextEncoding::NarrowToWide (source + " is a Merlin source").c_str());

        AssertSucceeded (FileFormatTable::Decode (format, payload.bytes, FileFacts::From (*entry, kind), DecodeOptions(), decoded, error));
        AssertSucceeded (FileFormatTable::Export (decoded, text));
        AssertSucceeded (FileFormatTable::Import (format, kind, text, imported, error));
        Assert::IsTrue  (imported.bytes == payload.bytes, L"the file put back is the file copied out");
        AssertSucceeded (volume->Write (FilePath::Parse (folder + "CASSO.S"), imported, written));
        AssertSucceeded (VolumeImage::Save (bytes, "merlin.dsk", written, saved, reason));

        exported.assign (text.begin(), text.end());

        //  The text file starts with UTF-8's byte order mark.
        if (exported.starts_with ("\xEF\xBB\xBF"))
        {
            exported.erase (0, 3);
        }

        while (!exported.empty() && isspace ((unsigned char) exported.back()))
        {
            exported.pop_back();
        }

        putBack.disk      = saved;
        putBack.firstWord = exported.substr (0, exported.find_first_of (" \r\n"));
        putBack.lastWord  = exported.substr (exported.find_last_of (" \r\n") + 1);

        return putBack;
    }
};
