#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoExplorer/Model/CrossVolumeCopy.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergTests
//
//  Gutenberg, Jr. on a //e in Casso, with its side A in drive 1 and a data
//  disk in drive 2. Gutenberg formats the data disk, and Casso copies a
//  ProDOS text file to it as Casso Explorer does. Gutenberg then writes a
//  document in sectors Casso's map marks free, and opens Casso's
//  document, adds to it and saves it again. Gutenberg draws its text on the
//  hi-res page, so the checks read the disk rather than the screen. The disk
//  comes from scripts/FetchStockDisks.ps1.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GutenbergTests)
{
public:

    static constexpr int       kSlot6      = 6;
    static constexpr int       kDrive2     = 1;
    static constexpr uint64_t  kMillion    = 1'000'000ULL;
    static constexpr uint64_t  kKeyCycles  = 300'000ULL;
    static constexpr Byte      kReturn     = 0x0D;
    static constexpr Byte      kEscape     = 0x1B;
    static constexpr Byte      kCtrlQ      = 0x11;
    static constexpr Byte      kCtrlU      = 0x15;



    static void Run (TestMachine & machine, uint64_t millions)
    {
        machine.RunCycles (millions * kMillion);
    }


    static void Press (TestMachine & machine, Byte key)
    {
        machine.GetRefs().iieKeyboard->PressKey (key);
        machine.RunCycles (kKeyCycles);
    }


    static void Type (TestMachine & machine, const std::string & text)
    {
        for (char ch : text)
        {
            Press (machine, (Byte) ch);
        }
    }


    static void MountDrive2 (TestMachine & machine, const std::vector<Byte> & sectors)
    {
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot6, kDrive2, "data.dsk", DiskFormat::Dsk, sectors));
    }


    static std::vector<Byte> ReadDrive2 (TestMachine & machine)
    {
        DiskImage *         image = machine.GetDiskStore().GetImage (kSlot6, kDrive2);
        SectorDecodeReport  report;



        Assert::IsNotNull (image);

        return GuestSession::DecodeThroughTheDrive (*image, report);
    }


    //  Boot side A and answer yes to both opening questions. Gutenberg is then
    //  at READY:.
    static void BootToReady (TestMachine & machine, const std::vector<Byte> & data)
    {
        GuestSession::Mount (machine, GuestSession::RequireGutenbergJrDisk());
        MountDrive2 (machine, data);

        Run   (machine, 60);
        Type  (machine, "Y");
        Run   (machine, 30);
        Type  (machine, "Y");
        Run   (machine, 40);
    }


    //  Utility 3, FORMAT DATA DISKS, on drive 2.
    static void FormatDrive2 (TestMachine & machine, const std::string & name)
    {
        Press (machine, kCtrlU);
        Type  (machine, "32");
        Type  (machine, name);
        Press (machine, kReturn);
        Run   (machine, 150);
        Press (machine, kEscape);
        Run   (machine, 10);
    }


    //  The text of a document up to its first $00, in high ASCII.
    static std::vector<Byte> ReadDocument (const std::vector<Byte> & sectors, const char * name)
    {
        FilePayload  payload;



        AssertSucceeded (GutenbergVolume (sectors).Read (FilePath::Parse (name), payload));

        return std::vector<Byte> (payload.bytes.begin(), std::find (payload.bytes.begin(), payload.bytes.end(), GutenbergVolume::kDocumentEnd));
    }


    TEST_METHOD (ADocumentCassoWrites_OpensAndSavesInGutenberg)
    {
        TestMachine            machine ("Apple2e");
        std::vector<Byte>      formatted;
        std::vector<Byte>      written;
        std::vector<Byte>      saved;
        std::vector<Byte>      expected;
        std::vector<Byte>      text;
        std::string            body;
        FilePayload            payload;
        std::wstring           note;
        VolumeListing          listing;
        VolumeIntegrityReport  report;



        BootToReady  (machine, std::vector<Byte> (GuestSession::kImageBytes, 0));
        FormatDrive2 (machine, "CASSO");

        formatted = ReadDrive2 (machine);
        Assert::IsTrue (GutenbergVolume::IsFound (formatted), L"Gutenberg formatted the data disk");

        //  Three sectors of ProDOS text, copied as Casso Explorer copies it.
        for (int i = 0; i < 12; i++)
        {
            body += std::format ("Line {} of a document Casso wrote for Gutenberg.\n", i + 1);
        }

        for (char ch : body)
        {
            payload.bytes.push_back ((ch == '\n') ? (Byte) 0x8D : (Byte) (ch | 0x80));
        }

        payload.type = ProDosVolume::kTypeText;
        CrossVolumeCopy::Convert (VolumeKind::ProDos, VolumeKind::Gutenberg, "FROMCASSO", payload, note);

        Assert::IsTrue  (payload.type == GutenbergVolume::kTypeDocument);
        AssertSucceeded (GutenbergVolume (formatted).Write (FilePath::Parse ("FROMCASSO"), payload, written));
        AssertSucceeded (GutenbergVolume (written).Enumerate (listing));
        Assert::AreEqual (3u, listing.entries[1].sizeUnits);

        MountDrive2 (machine, written);

        //  A document of Gutenberg's, whose sectors come from the map Casso
        //  wrote.
        Type  (machine, "2");
        Press (machine, kReturn);
        Run   (machine, 5);
        Type  (machine, "LETTER");
        Press (machine, kReturn);
        Run   (machine, 30);
        Press (machine, kReturn);
        Run   (machine, 30);
        Type  (machine, "I");
        Type  (machine, "Gutenberg wrote this.");
        Press (machine, kEscape);
        Run   (machine, 5);
        Press (machine, kCtrlQ);
        Run   (machine, 80);

        //  Open Casso's document, insert at its start and quit, which saves it.
        Press (machine, kReturn);
        Run   (machine, 5);
        Type  (machine, "FROMCASSO");
        Press (machine, kReturn);
        Run   (machine, 40);
        Type  (machine, "I");
        Type  (machine, "Edited. ");
        Press (machine, kEscape);
        Run   (machine, 5);
        Press (machine, kCtrlQ);
        Run   (machine, 80);

        saved = ReadDrive2 (machine);

        for (char ch : std::string ("Edited. "))
        {
            expected.push_back ((Byte) (ch | 0x80));
        }

        expected.insert (expected.end(), payload.bytes.begin(), payload.bytes.end());
        text = ReadDocument (saved, "FROMCASSO");

        Assert::IsTrue (text == expected, L"Gutenberg read the whole document and saved it with the edit");

        expected.clear();

        for (char ch : std::string ("Gutenberg wrote this."))
        {
            expected.push_back ((Byte) (ch | 0x80));
        }

        Assert::IsTrue (ReadDocument (saved, "LETTER") == expected, L"Gutenberg's document is intact beside Casso's");

        AssertSucceeded (GutenbergVolume (saved).BuildIntegrityReport (report));
        Assert::IsTrue  (report.IsClean(), L"the map matches the files after Gutenberg's save");
    }
};
