#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  A2PicoCardActionsTests
//
//  An A2Pico card's A2retroNET drives read, set, repaired and written, its
//  images listed and converted, and its drives updated as files on it are
//  renamed, moved and deleted, over an in-memory card at E:.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (A2PicoCardActionsTests)
{
public:

    struct Host
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };
        CassoExplorerActions  actions { browser, fs };

        Host()
        {
            SeedFixture ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        }

        static std::string Narrow (const std::wstring & path)
        {
            return std::string (path.begin(), path.end());
        }

        void SeedBytes (const std::vector<Byte> & bytes, const std::wstring & path)
        {
            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));
            io.files[Narrow (path)]  = bytes;
            io.stamps[Narrow (path)] = FileStamp { bytes.size(), 100 };
        }

        void SeedFixture (const char * fixture, const std::wstring & path)
        {
            FixtureProvider    fixtures;
            std::vector<Byte>  bytes;

            AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
            SeedBytes (bytes, path);
        }
    };


    static std::vector<Byte> Bytes (const char * text)
    {
        return std::vector<Byte> (text, text + strlen (text));
    }


    //  An image on a card with A2retroNET.txt in its root, set as the first
    //  drive with no image. A .dsk isn't offered.
    TEST_METHOD (AddToA2retroNet_SetsTheFirstFreeDrive)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        std::string                    text;



        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        host.SeedBytes   (Bytes ("[settings]\r\nbootdelay=5\r\n[drives]\r\nnumber=4\r\n1=system.hdv\r\n"), L"C:\\A2retroNET.txt");

        Assert::IsTrue  (host.actions.CanAddToA2retroNet (L"C:\\Disks\\prodos.po"));

        outcome = host.actions.AddToA2retroNet (L"C:\\Disks\\prodos.po");

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (std::wstring (L"prodos.po is drive 2."), outcome.message);

        text.assign (host.io.files["C:\\A2retroNET.txt"].begin(), host.io.files["C:\\A2retroNET.txt"].end());
        Assert::AreEqual (std::string ("[settings]\nbootdelay=5\n[drives]\nnumber=4\n1=system.hdv\n2=Disks/prodos.po\n"), text, L"no lines added for drives with no image");

        outcome = host.actions.AddToA2retroNet (L"C:\\Disks\\prodos.po");
        Assert::AreEqual (std::wstring (L"prodos.po is drive 2 already."), outcome.message);

        outcome = host.actions.AddToA2retroNet (L"C:\\Disks\\dos33.dsk");
        Assert::IsFalse  (outcome.Succeeded());
        Assert::IsTrue   (outcome.message.find (L"A2retroNET reads only .po, .hdv and .2mg images.") != std::wstring::npos, outcome.message.c_str());
    }


    //  Each drive in use, with its image's size and the problem the firmware
    //  would have opening it. A path on the other device can't be checked
    //  from this card.
    TEST_METHOD (ReadA2PicoCard_ListsEachDriveAndItsProblem)
    {
        Host                          host;
        ActionsA2retroNet::Card       card;
        std::vector<Byte>             image (280 * 512, 0);



        host.SeedBytes (Bytes ("# card\n[drives]\nnumber=6\n1=system.po\n2=missing.po\n3=games/zork.dsk\n4=usb:/floppy.po\n6=/locked.hdv\n7=ignored.po\n"), L"E:\\A2retroNET.txt");
        host.SeedBytes (image, L"E:\\system.po");
        host.SeedBytes (image, L"E:\\games\\zork.dsk");
        host.SeedBytes (image, L"E:\\locked.hdv");
        AssertSucceeded (host.fs.SetReadOnlyAttribute (L"E:\\locked.hdv", true));

        Assert::IsTrue  (host.actions.IsA2PicoCard (L"E:\\"));
        Assert::IsFalse (host.actions.IsA2PicoCard (L"C:\\Disks"));

        AssertSucceeded (host.actions.ReadA2PicoCard (L"E:\\", card));

        Assert::AreEqual ((size_t) 6, card.drives.size(), L"drives past number= aren't in use");
        Assert::IsTrue   (card.drives[0].isPresent);
        Assert::AreEqual ((uint64_t) image.size(), card.drives[0].bytes);
        Assert::IsTrue   (card.drives[0].problem.empty());
        Assert::AreEqual (std::wstring (L"missing.po isn't on the card."), card.drives[1].problem);
        Assert::AreEqual (std::wstring (L"A2retroNET reads only .po, .hdv and .2mg images."), card.drives[2].problem);
        Assert::AreEqual (std::wstring (L"usb:/floppy.po isn't on this card. It may be on the other storage device."), card.drives[3].problem);
        Assert::IsTrue   (card.drives[4].path.empty());
        Assert::IsTrue   (card.drives[4].problem.empty(), L"a drive with no image is like an empty drive");
        Assert::IsTrue   (card.drives[5].isWriteProtected);

        Assert::AreEqual (std::wstring (L"A2Pico card. 5 of its 6 A2retroNET drives have an image. Drive 2: missing.po isn't on the card. Drive 3: A2retroNET reads only .po, .hdv and .2mg images. Drive 4: usb:/floppy.po isn't on this card. It may be on the other storage device."),
                          ActionsA2retroNet::Summarize (card));
    }


    //  With CR LF line ends, each path ends in a CR and no image opens.
    //  Repair writes the file with LF line ends and its comments in place.
    TEST_METHOD (RepairA2retroNetConfig_RemovesTheCarriageReturns)
    {
        Host                           host;
        ActionsA2retroNet::Card        card;
        CassoExplorerActions::Outcome  outcome;
        std::string                    text;



        host.SeedBytes (Bytes ("# card\r\n[drives]\r\nnumber=2\r\n1=\r\n"), L"E:\\A2retroNET.txt");

        AssertSucceeded (host.actions.ReadA2PicoCard (L"E:\\", card));
        Assert::IsTrue   (card.config.hasCarriageReturns);
        Assert::AreEqual (std::wstring (L"A2Pico card. 0 of its 2 A2retroNET drives have an image. A2retroNET.txt has CR LF line ends, so the A2Pico doesn't open any image. Repair writes it with LF line ends."),
                          ActionsA2retroNet::Summarize (card));

        outcome = host.actions.RepairA2retroNetConfig (L"E:\\");

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        text.assign (host.io.files["E:\\A2retroNET.txt"].begin(), host.io.files["E:\\A2retroNET.txt"].end());
        Assert::AreEqual (std::string ("# card\n[drives]\nnumber=2\n1=\n"), text);
    }


    //  The images A2retroNET drives offers for a drive: each .po, .hdv and
    //  .2mg on the card, in its folders too, as the path the file has.
    TEST_METHOD (ListA2PicoImages_FindsTheImagesOnTheCard)
    {
        Host                      host;
        std::vector<std::string>  images;
        std::vector<Byte>         image (280 * 512, 0);



        host.SeedBytes (Bytes ("[drives]\n"), L"E:\\A2retroNET.txt");
        host.SeedBytes (image, L"E:\\system.po");
        host.SeedBytes (image, L"E:\\Games\\zork.PO");
        host.SeedBytes (image, L"E:\\Games\\Infocom\\deadline.hdv");
        host.SeedBytes (image, L"E:\\Games\\notes.txt");
        host.SeedBytes (image, L"E:\\Big.2mg");
        host.SeedBytes (image, L"E:\\dos33.dsk");

        AssertSucceeded (host.actions.ListA2PicoImages (L"E:\\", images));

        Assert::AreEqual ((size_t) 4, images.size());
        Assert::AreEqual (std::string ("Big.2mg"),                    images[0]);
        Assert::AreEqual (std::string ("Games/Infocom/deadline.hdv"), images[1]);
        Assert::AreEqual (std::string ("Games/zork.PO"),              images[2]);
        Assert::AreEqual (std::string ("system.po"),                  images[3]);
    }


    TEST_METHOD (SaveA2retroNetConfig_WritesTheDrivesAndTheComments)
    {
        Host                           host;
        A2retroNetConfig               config = A2retroNetConfig::Parse ("# mine\n[drives]\n1=a.po\n");
        CassoExplorerActions::Outcome  outcome;
        std::string                    text;



        host.SeedBytes (Bytes ("# mine\n[drives]\n1=a.po\n"), L"E:\\A2retroNET.txt");

        config.paths[1] = "b.po";
        config.number   = 4;
        outcome         = host.actions.SaveA2retroNetConfig (L"E:\\", config);

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        text.assign (host.io.files["E:\\A2retroNET.txt"].begin(), host.io.files["E:\\A2retroNET.txt"].end());
        Assert::AreEqual (std::string ("# mine\n[drives]\nnumber=4\n1=a.po\n2=b.po\n"), text);
    }


    //  When a drive's image is renamed or moved on the card, the drive is set
    //  to its new path. A move off the card or a delete empties the drive, and
    //  a file that is no drive's image changes nothing.
    TEST_METHOD (NoteA2Pico_SetsEachDriveToItsImagesNewPath)
    {
        Host  host;



        host.SeedBytes (Bytes ("# card\n[drives]\nnumber=4\n1=system.po\n2=games/zork.po\n3=other.po\n"), L"E:\\A2retroNET.txt");

        host.actions.GetA2retroNet().NoteMove (L"E:\\games\\zork.po", L"E:\\infocom\\zork1.po");
        host.actions.GetA2retroNet().NoteMove (L"E:\\other.po", L"C:\\Disks\\other.po");
        host.actions.GetA2retroNet().NoteMove (L"E:\\unset.po", L"E:\\unset-too.po");

        Assert::AreEqual (std::wstring (L"system.po is drive 1 of this A2Pico card. Deleting it empties the drive."),
                          host.actions.GetA2retroNet().DescribeDrives ({ L"E:\\system.po", L"E:\\unset.po" }));
        Assert::IsTrue   (host.actions.GetA2retroNet().DescribeDrives ({ L"E:\\unset.po" }).empty());

        host.actions.GetA2retroNet().NoteDelete (L"E:\\system.po");

        Assert::AreEqual (std::string ("# card\n[drives]\nnumber=4\n1=\n2=infocom/zork1.po\n3=\n"), std::string (host.io.files["E:\\A2retroNET.txt"].begin(), host.io.files["E:\\A2retroNET.txt"].end()));
    }


    //  A drop onto a folder is done by Windows, which may move the files
    //  after the drop returns. A file whose source is gone and whose target
    //  is there is a move, and its drive takes the new path. A copy changes
    //  nothing.
    TEST_METHOD (ExpectedMoves_SetTheDrivesOnceTheFilesMove)
    {
        Host                      host;
        std::vector<Byte>         image (280 * 512, 0);
        A2retroNetConfig          config;
        ActionsA2retroNet::Card   card;



        host.SeedBytes (Bytes ("[drives]\n1=games/zork.po\n2=other.po\n"), L"E:\\A2retroNET.txt");
        host.SeedBytes (image, L"E:\\games\\zork.po");
        host.SeedBytes (image, L"E:\\other.po");

        host.actions.GetA2retroNet().ExpectMoves ({ L"E:\\games\\zork.po", L"E:\\other.po" }, L"E:\\infocom");
        host.actions.GetA2retroNet().ResolveMoves();

        AssertSucceeded  (host.actions.ReadA2PicoCard (L"E:\\", card));
        Assert::AreEqual (std::string ("games/zork.po"), card.config.paths[0], L"nothing has moved yet");

        AssertSucceeded (host.fs.Delete (L"E:\\games\\zork.po"));
        host.SeedBytes  (image, L"E:\\infocom\\zork.po");
        host.SeedBytes  (image, L"E:\\infocom\\other.po");
        host.actions.GetA2retroNet().ResolveMoves();

        AssertSucceeded  (host.actions.ReadA2PicoCard (L"E:\\", card));
        Assert::AreEqual (std::string ("infocom/zork.po"), card.config.paths[0]);
        Assert::AreEqual (std::string ("other.po"),        card.config.paths[1], L"a copy");
    }


    //  A ProDOS volume in a DOS-order .dsk, which A2retroNET doesn't read,
    //  is offered as a .po beside it with the same blocks. A DOS 3.3 disk
    //  isn't offered, and neither is a .dsk with a .po beside it.
    TEST_METHOD (ConvertA2PicoImage_WritesAProDosVolumeAsAPo)
    {
        Host                           host;
        FixtureProvider                fixtures;
        std::vector<Byte>              po;
        std::vector<Byte>              dsk (NibblizationLayer::kImageByteSize, 0);
        std::vector<std::wstring>      found;
        std::wstring                   written;
        CassoExplorerActions::Outcome  outcome;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", po));

        for (int block = 0; block < (int) (po.size() / 512); block++)
        {
            for (size_t i = 0; i < 512; i++)
            {
                dsk[ProDosSkeleton::GetBlockByteOffset (block, i)] = po[(size_t) block * 512 + i];
            }
        }

        host.SeedBytes   (Bytes ("[drives]\n"), L"E:\\A2retroNET.txt");
        host.SeedBytes   (dsk, L"E:\\games\\zork.dsk");
        host.SeedFixture ("CassoExplorer/dos33.dsk", L"E:\\games\\dos33.dsk");

        AssertSucceeded  (host.actions.GetA2retroNet().FindConvertible (L"E:\\games", found));
        Assert::AreEqual ((size_t) 1, found.size());
        Assert::AreEqual (std::wstring (L"E:\\games\\zork.dsk"), found[0]);

        outcome = host.actions.GetA2retroNet().ConvertToPo (L"E:\\games\\zork.dsk", written);

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (std::wstring (L"E:\\games\\zork.po"), written);
        Assert::IsTrue   (host.io.files["E:\\games\\zork.po"] == po, L"the same blocks in ProDOS order");

        AssertSucceeded (host.fs.WriteAllText (L"E:\\games\\zork.po", std::string (po.begin(), po.end())));
        AssertSucceeded (host.actions.GetA2retroNet().FindConvertible (L"E:\\games", found));
        Assert::IsTrue  (found.empty(), L"its .po is beside it");
    }


    //  A card just initialized for an A2Pico gets the firmware's defaults. A
    //  card that has the file already is left as it is.
    TEST_METHOD (CreateA2retroNetConfig_WritesTheDefaultsOnce)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        std::string                    text;



        outcome = host.actions.CreateA2retroNetConfig (L"E:\\");

        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());
        text.assign (host.io.files["E:\\A2retroNET.txt"].begin(), host.io.files["E:\\A2retroNET.txt"].end());
        Assert::AreEqual (std::string ("[settings]\nbootdelay=3\n[drives]\nnumber=8\n1=\n2=\n3=\n4=\n5=\n6=\n7=\n8=\n"), text);

        host.SeedBytes (Bytes ("# mine\n[drives]\n1=a.po\n"), L"E:\\A2retroNET.txt");
        outcome = host.actions.CreateA2retroNetConfig (L"E:\\");

        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual (0, outcome.written);
        text.assign (host.io.files["E:\\A2retroNET.txt"].begin(), host.io.files["E:\\A2retroNET.txt"].end());
        Assert::AreEqual (std::string ("# mine\n[drives]\n1=a.po\n"), text);
    }
};
