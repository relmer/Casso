#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/CassqueActions.h"
#include "Cassque/CassqueNewDiskDialog.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActionsTests
//
//  How a host file is planned into an image, and the menu verbs run end to
//  end over the scratch DOS 3.3 image: the catalog after the write, the
//  written callback, and the host file a get produces.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueActionsTests)
{
public:

    struct Host
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        CassqueBrowser      browser { fs, io };
        CassqueActions      actions { browser, fs };
        std::vector<std::wstring>  written;

        Host()
        {
            SeedFixture ("Cassque/dos33.dsk", L"C:\\Disks\\dos33.dsk");

            browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks" });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring &) { return true; });

            actions.SetOnImageWritten ([this] (const std::wstring & path) { written.push_back (path); });
        }

        static std::string Narrow (const std::wstring & path)
        {
            std::string  out;

            for (wchar_t c : path)
            {
                out.push_back ((char) c);
            }

            return out;
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

        void OpenImage()
        {
            std::vector<DxuiTreeNode>  roots;

            browser.GetTreeRoots (roots);

            std::wstring  folder = browser.GetTreeChildren (roots[0].id)[0].id;

            AssertSucceeded (browser.SelectTreeNode (folder));
            Assert::IsTrue (browser.OpenRow (Find (L"dos33.dsk")));
        }

        int Find (const wchar_t * name)
        {
            for (size_t i = 0; i < browser.GetRows().size(); i++)
            {
                if (browser.GetRows()[i].name == name)
                {
                    return (int) i;
                }
            }

            return -1;
        }
    };



    static std::vector<Byte> Bytes (const char * text)
    {
        return std::vector<Byte> (text, text + strlen (text));
    }



    TEST_METHOD (PlanPut_ConvertedSuffixesPickTheConversion)
    {
        CassqueActions::PutPlan  plan;

        plan = CassqueActions::PlanPut (L"C:\\x\\HELLO.Applesoft BASIC.txt", Bytes ("10 PRINT\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassqueActions::Encoding::Basic);
        Assert::AreEqual (std::string ("HELLO"), plan.catalogName);

        plan = CassqueActions::PlanPut (L"NOTES.Text.txt", Bytes ("anything"), VolumeKind::ProDos);
        Assert::IsTrue   (plan.encoding == CassqueActions::Encoding::Text);
        Assert::IsFalse  (plan.usePayload);
    }


    TEST_METHOD (PlanPut_CiderPressSuffixWritesATypedPayload)
    {
        std::vector<Byte>        bytes (100, 0xEA);
        CassqueActions::PutPlan  pro = CassqueActions::PlanPut (L"PIC#062000", bytes, VolumeKind::ProDos);
        CassqueActions::PutPlan  dos = CassqueActions::PlanPut (L"PIC#062000", bytes, VolumeKind::Dos33);

        Assert::IsTrue   (pro.usePayload);
        Assert::AreEqual ((int) ProDosVolume::kTypeBinary, (int) pro.payload.type);
        Assert::IsTrue   (pro.payload.hasLoadAddress);
        Assert::AreEqual ((int) 0x2000, (int) pro.payload.loadAddress);

        Assert::IsTrue   (dos.usePayload);
        Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) dos.payload.type);
        Assert::IsFalse  (dos.payload.hasAuxType);
    }


    TEST_METHOD (PlanPut_BareNameDecidesByContent)
    {
        std::vector<Byte>        picture (8192, 0x55);
        CassqueActions::PutPlan  plan;

        plan = CassqueActions::PlanPut (L"prog.bas", Bytes ("10 PRINT \"HI\"\n20 END\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassqueActions::Encoding::Basic);
        Assert::AreEqual (std::string ("PROG"), plan.catalogName);

        plan = CassqueActions::PlanPut (L"readme.md", Bytes ("Just words.\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassqueActions::Encoding::Text);

        plan = CassqueActions::PlanPut (L"screen.pic", picture, VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassqueActions::Encoding::Verbatim);
        Assert::IsTrue   (plan.guessedAddress);
        Assert::AreEqual ((int) 0x2000, (int) plan.loadAddress);
    }


    TEST_METHOD (CatalogName_FollowsEachFileSystemsRules)
    {
        Assert::AreEqual (std::string ("A1ST.FILE"),       CassqueActions::MakeCatalogName (L"1st file.txt", VolumeKind::ProDos));
        Assert::AreEqual (std::string ("MY FILE.V2"),      CassqueActions::MakeCatalogName (L"my file,v2.txt", VolumeKind::Dos33));
        Assert::AreEqual (std::string ("ABCDEFGHIJKLMNO"), CassqueActions::MakeCatalogName (L"abcdefghijklmnopqrs.bin", VolumeKind::ProDos));
    }


    TEST_METHOD (Verbs_DependOnWhatIsSelected)
    {
        Host                                  host;
        std::vector<CassqueActions::Verb>     verbs;

        host.OpenImage();
        host.browser.GoUp();

        host.browser.SetSelectedRows ({ host.Find (L"dos33.dsk") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::InsertDrive1) != verbs.end());

        host.browser.OpenRow (host.Find (L"dos33.dsk"));
        host.browser.SetSelectedRows ({ host.Find (L"HELLO") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::Rename) != verbs.end());
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::InsertDrive1) != verbs.end());
    }


    TEST_METHOD (AHostFile_OpensInItsOwnProgram_AndEveryHostItemHasTheShellsMenu)
    {
        Host                               host;
        std::vector<CassqueActions::Verb>  verbs;
        auto                               has = [&verbs] (CassqueActions::Verb verb) { return std::find (verbs.begin(), verbs.end(), verb) != verbs.end(); };

        host.SeedBytes (Bytes ("hello"), L"C:\\Disks\\readme.txt");
        host.OpenImage();
        host.browser.GoUp();

        host.browser.SetSelectedRows ({ host.Find (L"readme.txt") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue (has (CassqueActions::Verb::Open));
        Assert::IsTrue (has (CassqueActions::Verb::OpenWith));
        Assert::IsTrue (has (CassqueActions::Verb::MoreOptions));
        Assert::IsTrue (has (CassqueActions::Verb::Cut));
        Assert::IsTrue (has (CassqueActions::Verb::Copy));
        Assert::IsTrue (has (CassqueActions::Verb::Delete));
        Assert::IsTrue (has (CassqueActions::Verb::Rename));
        Assert::IsFalse (has (CassqueActions::Verb::Paste), L"A file is not somewhere to paste");
        Assert::IsTrue  (has (CassqueActions::Verb::Share));

        //  The folder's background takes a paste.
        host.browser.SetSelectedRows ({});
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue  (has (CassqueActions::Verb::Paste));
        Assert::IsFalse (has (CassqueActions::Verb::Cut));

        //  An image opens here, so it gets no program to open it with.
        host.browser.SetSelectedRows ({ host.Find (L"dos33.dsk") });
        verbs = host.actions.GetListVerbs();
        Assert::IsFalse (has (CassqueActions::Verb::OpenWith));
        Assert::IsTrue  (has (CassqueActions::Verb::MoreOptions));

        //  Nothing inside an image is a real file.
        host.browser.OpenRow (host.Find (L"dos33.dsk"));
        host.browser.SetSelectedRows ({ host.Find (L"HELLO") });
        verbs = host.actions.GetListVerbs();
        Assert::IsFalse (has (CassqueActions::Verb::OpenWith));
        Assert::IsFalse (has (CassqueActions::Verb::MoreOptions));
        Assert::IsFalse (has (CassqueActions::Verb::Cut), L"Entries in an image are copied out with Get");
    }


    TEST_METHOD (Delete_RemovesTheEntryAndReportsTheWrite)
    {
        Host  host;

        host.OpenImage();
        host.browser.SetSelectedRows ({ host.Find (L"NOTES") });

        CassqueActions::Outcome  outcome = host.actions.DeleteSelected();

        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual (-1, host.Find (L"NOTES"));
        Assert::AreEqual ((size_t) 1, host.written.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), host.written[0]);
    }


    TEST_METHOD (Rename_ChangesTheCatalogName)
    {
        Host  host;

        host.OpenImage();
        host.browser.SetSelectedRows ({ host.Find (L"ODD") });

        Assert::IsTrue   (host.actions.RenameSelected (L"even").Succeeded());
        Assert::AreEqual (-1, host.Find (L"ODD"));
        Assert::AreNotEqual (-1, host.Find (L"EVEN"));
    }


    TEST_METHOD (Get_WritesTheDescriptiveHostName)
    {
        Host  host;

        host.OpenImage();
        host.browser.SetSelectedRows ({ host.Find (L"HELLO"), host.Find (L"ODD") });

        CassqueActions::Outcome  outcome = host.actions.GetSelected (L"C:\\Out", HostFileNaming::Style::Descriptive);

        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual (2, outcome.written);
        Assert::IsTrue   (host.io.files.count ("C:\\Out\\HELLO.Applesoft BASIC.txt") == 1);
        Assert::IsTrue   (host.written.empty());
    }


    TEST_METHOD (Put_AddsHostFilesByTheirContent)
    {
        Host  host;

        host.SeedBytes (Bytes ("10 PRINT \"PUT\"\n20 END\n"), L"C:\\In\\putprog.bas");
        host.OpenImage();

        CassqueActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\putprog.bas" });

        Assert::IsTrue      (outcome.Succeeded());
        Assert::AreNotEqual (-1, host.Find (L"PUTPROG"));
        Assert::AreEqual    ((size_t) 1, host.written.size());
    }


    TEST_METHOD (InsideAProDosSubdirectory_FilesArePutRenamedAndDeleted)
    {
        Host                               host;
        std::vector<CassqueActions::Verb>  verbs;
        std::vector<DxuiTreeNode>          roots;

        host.SeedFixture ("Cassque/prodos.po", L"C:\\Disks\\prodos.po");
        host.SeedBytes   (Bytes ("10 PRINT \"SUB\"\n20 END\n"), L"C:\\In\\subprog.bas");

        host.browser.GetTreeRoots (roots);
        AssertSucceeded (host.browser.SelectTreeNode (host.browser.GetTreeChildren (roots[0].id)[0].id));
        Assert::IsTrue  (host.browser.OpenRow (host.Find (L"prodos.po")));

        Assert::IsTrue  (host.actions.CreateFolder (L"NESTED").Succeeded());
        host.browser.Reload (false);
        Assert::IsTrue  (host.browser.OpenRow (host.Find (L"NESTED")));

        //  Put goes into the open subdirectory, not the volume directory.
        Assert::IsTrue      (host.actions.PutFiles ({ L"C:\\In\\subprog.bas" }).Succeeded());
        Assert::AreNotEqual (-1, host.Find (L"SUBPROG"));

        host.browser.SetSelectedRows ({ host.Find (L"SUBPROG") });
        verbs = host.actions.GetListVerbs();

        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::Delete) != verbs.end());
        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::Rename) != verbs.end());
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::Boot)   != verbs.end(), L"Nothing boots from a subdirectory");
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassqueActions::Verb::Format) != verbs.end(), L"A subdirectory is not the disk");

        Assert::IsTrue      (host.actions.RenameSelected (L"moved").Succeeded());
        Assert::AreEqual    (-1, host.Find (L"SUBPROG"));
        Assert::AreNotEqual (-1, host.Find (L"MOVED"));

        host.browser.SetSelectedRows ({ host.Find (L"MOVED") });
        Assert::IsTrue   (host.actions.DeleteSelected().Succeeded());
        Assert::AreEqual (-1, host.Find (L"MOVED"));
    }


    TEST_METHOD (ANewItemsName_IsTheFirstOneNotInUse)
    {
        Assert::AreEqual (std::wstring (L"New folder"),     CassqueActions::MakeUnusedName (L"New folder", { L"a.txt" }, true));
        Assert::AreEqual (std::wstring (L"New folder (2)"), CassqueActions::MakeUnusedName (L"New folder", { L"new FOLDER" }, true),
                          L"Case does not tell names apart");
        Assert::AreEqual (std::wstring (L"New folder (3)"), CassqueActions::MakeUnusedName (L"New folder", { L"New folder", L"New folder (2)" }, true));

        Assert::AreEqual (std::wstring (L"NEW.FOLDER.2"),   CassqueActions::MakeUnusedName (L"NEW.FOLDER", { L"NEW.FOLDER" }, false));
        Assert::AreEqual (std::wstring (L"ABCDEFGHIJKLM.2"), CassqueActions::MakeUnusedName (L"ABCDEFGHIJKLMNO", { L"ABCDEFGHIJKLMNO" }, false),
                          L"Within ProDOS's fifteen characters");
    }


    TEST_METHOD (SpaceIsCountedInBlocksAndSectors)
    {
        Assert::AreEqual ((uint64_t) 1,   CassqueActions::CountUnitsForFile (VolumeKind::ProDos, 0),      L"A seedling file");
        Assert::AreEqual ((uint64_t) 1,   CassqueActions::CountUnitsForFile (VolumeKind::ProDos, 512));
        Assert::AreEqual ((uint64_t) 3,   CassqueActions::CountUnitsForFile (VolumeKind::ProDos, 513),    L"Two data blocks and an index block");
        Assert::AreEqual ((uint64_t) 2,   CassqueActions::CountUnitsForFile (VolumeKind::Dos33, 100),     L"A sector and its track/sector list");
        Assert::AreEqual ((uint64_t) 125, CassqueActions::CountUnitsForFile (VolumeKind::Dos33, 123 * 256), L"123 sectors need a second list");
    }


    TEST_METHOD (ASelectionTooLarge_IsRefusedBeforeTheDiskIsMade)
    {
        Host  host;

        host.SeedBytes (std::vector<Byte> (200 * 1024, 0x55), L"C:\\In\\big.bin");
        host.SeedBytes (Bytes ("small"),                       L"C:\\In\\small.txt");

        Assert::IsTrue  (host.actions.CheckFitsNewDisk (VolumeKind::ProDos, { L"C:\\In\\small.txt" }).empty());
        Assert::IsFalse (host.actions.CheckFitsNewDisk (VolumeKind::ProDos, { L"C:\\In\\big.bin" }).empty());
        Assert::IsFalse (host.actions.CheckFitsNewDisk (VolumeKind::Dos33,  { L"C:\\In" }).empty(), L"DOS 3.3 has no folders");
    }


    TEST_METHOD (AFolderGoesIntoAProDosImageAsADirectory_AndIsRefusedByDos33)
    {
        Host                     host;
        CassqueActions::Outcome  outcome;

        host.SeedFixture ("Cassque/prodos.po", L"C:\\Disks\\prodos.po");
        host.SeedBytes   (Bytes ("10 PRINT \"IN\"\n20 END\n"), L"C:\\In\\Games\\inner.bas");

        outcome = host.actions.PutInto (L"C:\\Disks\\prodos.po", VolumeKind::ProDos, "", { L"C:\\In\\Games" });
        Assert::IsTrue (outcome.Succeeded());

        {
            VolumeListing  listing;
            VolumeKind     kind = VolumeKind::Unknown;

            Assert::IsTrue (host.browser.GetOperations().List ("C:\\Disks\\prodos.po", "GAMES", listing, kind).Succeeded(),
                            L"The folder became a directory");
            Assert::AreEqual ((size_t) 1, listing.entries.size(), L"holding the file");
        }

        outcome = host.actions.PutInto (L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, "", { L"C:\\In\\Games" });
        Assert::IsFalse (outcome.Succeeded());
        Assert::IsTrue  (outcome.message.find (L"no folders") != std::wstring::npos);
    }


    TEST_METHOD (NewDiskChoices_MapToTheRunnersNames)
    {
        DiskOperations::NewDiskRequest  request = CassqueNewDiskChoices::MakeRequest (1, 2, L"MYVOL", true);

        Assert::AreEqual (std::string ("prodos"), request.formatName);
        Assert::AreEqual (std::string ("po"),     request.containerType);
        Assert::AreEqual (std::string ("MYVOL"),  request.volumeName);
        Assert::IsTrue   (request.bootable);

        request = CassqueNewDiskChoices::MakeRequest (2, 99, L"", true);
        Assert::AreEqual (std::string ("none"), request.formatName);
        Assert::AreEqual (std::string ("woz"),  request.containerType);
        Assert::IsFalse  (request.bootable);

        Assert::AreEqual (std::wstring (L"Games.dsk"), CassqueNewDiskChoices::ApplyExtension (L"Games", 1));
        Assert::AreEqual (std::wstring (L"Games.po"),  CassqueNewDiskChoices::ApplyExtension (L"Games.po", 1));
    }


    //  The new-disk dialog checks its fields as they are typed: a file name the
    //  host can hold and that is not taken, and a volume the format can use.
    TEST_METHOD (NewDiskChoices_ValidateFieldsAsTyped)
    {
        auto  noneTaken = [] (const std::wstring &) { return false; };
        auto  taken     = [] (const std::wstring & name) { return name == L"Games.dsk"; };

        Assert::IsTrue  (CassqueNewDiskChoices::ValidateFileName (L"Games", 1, noneTaken).empty());
        Assert::IsFalse (CassqueNewDiskChoices::ValidateFileName (L"",      1, noneTaken).empty(), L"A name is required");
        Assert::IsFalse (CassqueNewDiskChoices::ValidateFileName (L"a:b",   1, noneTaken).empty(), L"A name the host cannot hold is refused");
        Assert::IsFalse (CassqueNewDiskChoices::ValidateFileName (L"Games", 1, taken).empty(),     L"A taken name is refused, extension added");
        Assert::IsTrue  (CassqueNewDiskChoices::ValidateFileName (L"Games", 2, taken).empty(),     L"The same name as another image type is free");

        Assert::IsTrue  (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatDos33, L"").empty(), L"Blank takes the default");
        Assert::IsTrue  (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatDos33, L"254").empty());
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatDos33, L"0").empty());
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatDos33, L"255").empty());
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatDos33, L"12A").empty());

        Assert::IsTrue  (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatProDos, L"Games.2").empty(), L"Case does not matter");
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatProDos, L"2GAMES").empty(), L"A name starts with a letter");
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatProDos, L"MY GAMES").empty());
        Assert::IsFalse (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatProDos, L"ABCDEFGHIJKLMNOP").empty(), L"At most 15 characters");

        Assert::IsTrue  (CassqueNewDiskChoices::ValidateVolume (CassqueNewDiskChoices::kFormatNone, L"anything").empty(), L"An unformatted image has no volume to check");

        Assert::AreEqual ((size_t) 3,  CassqueNewDiskChoices::GetVolumeMaxLength (CassqueNewDiskChoices::kFormatDos33));
        Assert::AreEqual ((size_t) 15, CassqueNewDiskChoices::GetVolumeMaxLength (CassqueNewDiskChoices::kFormatProDos));
    }


    TEST_METHOD (CreateImage_WritesANewDiskAndRefusesAnExistingName)
    {
        Host                            host;
        DiskOperations::NewDiskRequest  request = CassqueNewDiskChoices::MakeRequest (0, 1, L"", false);

        host.OpenImage();
        host.browser.GoUp();

        Assert::IsTrue   (host.actions.CreateImage (L"C:\\Disks", L"fresh.dsk", request).Succeeded());
        Assert::IsTrue   (host.io.files.count ("C:\\Disks\\fresh.dsk") == 1);

        Assert::IsFalse  (host.actions.CreateImage (L"C:\\Disks", L"dos33.dsk", request).Succeeded());
    }


    TEST_METHOD (FormatImage_EmptiesTheCatalog)
    {
        Host  host;

        host.OpenImage();
        Assert::AreNotEqual (-1, host.Find (L"HELLO"));

        Assert::IsTrue   (host.actions.FormatImage (CassqueNewDiskChoices::MakeRequest (0, 1, L"", false)).Succeeded());
        Assert::AreEqual (-1, host.Find (L"HELLO"));
        Assert::AreEqual ((size_t) 1, host.written.size());
    }


    TEST_METHOD (Put_BinaryAsksForItsAddress)
    {
        Host               host;
        std::vector<Byte>  binary (300, 0x60);
        int                asked = 0;

        host.SeedBytes (binary, L"C:\\In\\routine.obj");
        host.OpenImage();

        CassqueActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\routine.obj" },
            [&asked] (const std::wstring &, Word suggested, Word & outAddress)
            {
                asked++;
                Assert::AreEqual ((int) 0x0803, (int) suggested);
                outAddress = 0x6000;
                return true;
            });

        Assert::IsTrue      (outcome.Succeeded());
        Assert::AreEqual    (1, asked);
        Assert::AreNotEqual (-1, host.Find (L"ROUTINE"));
    }


    TEST_METHOD (Put_DeclinedAddressSkipsTheFile)
    {
        Host               host;
        std::vector<Byte>  binary (300, 0x60);

        host.SeedBytes (binary, L"C:\\In\\skip.obj");
        host.OpenImage();

        CassqueActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\skip.obj" },
            [] (const std::wstring &, Word, Word &) { return false; });

        Assert::AreEqual (0, outcome.written);
        Assert::AreEqual (-1, host.Find (L"SKIP"));
    }


    TEST_METHOD (Address_ParsesTheUsualForms)
    {
        Word  address = 0;

        Assert::IsTrue   (CassqueActions::TryParseAddress (L"$2000", address));
        Assert::AreEqual ((int) 0x2000, (int) address);
        Assert::IsTrue   (CassqueActions::TryParseAddress (L" 0x0803 ", address));
        Assert::AreEqual ((int) 0x0803, (int) address);
        Assert::IsTrue   (CassqueActions::TryParseAddress (L"#768", address));
        Assert::AreEqual ((int) 768, (int) address);
        Assert::IsFalse  (CassqueActions::TryParseAddress (L"$10000", address));
        Assert::IsFalse  (CassqueActions::TryParseAddress (L"zz", address));
        Assert::IsFalse  (CassqueActions::TryParseAddress (L"", address));
    }


    TEST_METHOD (GoTo_ParsesAddressesAndOffsetsFromTheCaret)
    {
        int64_t  address = 0;
        int64_t  last    = 0;

        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"$08FF", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08FF, address);
        Assert::AreEqual (address, last, L"One address is a range of one");
        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"+10", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x0810, address, L"An offset is hex, as addresses are");
        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"+$A0", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08A0, address);
        Assert::IsTrue   (CassqueActions::TryParseGoTo (L" -#16 ", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x07F0, address, L"and # marks decimal");
        Assert::IsFalse  (CassqueActions::TryParseGoTo (L"+", 0x0800, address, last));
        Assert::IsFalse  (CassqueActions::TryParseGoTo (L"", 0x0800, address, last));
        Assert::IsFalse  (CassqueActions::TryParseGoTo (L"zz", 0x0800, address, last));
    }


    TEST_METHOD (GoTo_ReachesPastSixteenBitsInALargeFile)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"10000", 0, first, last));
        Assert::AreEqual ((int64_t) 0x10000, first, L"An offset is not an Apple II address and is not capped at $FFFF");
        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"+#1000000", 0x10, first, last));
        Assert::AreEqual ((int64_t) 1000016, first);
    }


    TEST_METHOD (GoTo_ParsesARangeByLastAddressOrByLength)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"$0803-$0810", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0803, first);
        Assert::AreEqual ((int64_t) 0x0810, last, L"A hyphen gives the last address");

        Assert::IsTrue   (CassqueActions::TryParseGoTo (L"$0803,+10", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0812, last, L"A comma and a plus give a length of sixteen");

        Assert::IsFalse  (CassqueActions::TryParseGoTo (L"$0810-$0803", 0x0800, first, last),
            L"A range cannot end before it starts");
        Assert::IsFalse  (CassqueActions::TryParseGoTo (L"$0803,+0", 0x0800, first, last),
            L"or be empty");
    }


    TEST_METHOD (Search_ParsesHexBytesAndQuotedText)
    {
        std::vector<Byte>  bytes;
        bool               isText = false;

        Assert::IsTrue  (CassqueActions::TryParseSearch (L" A9 00 8d ", bytes, isText));
        Assert::IsFalse (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 0xA9, 0x00, 0x8D });

        Assert::IsTrue  (CassqueActions::TryParseSearch (L"\"HI\"", bytes, isText));
        Assert::IsTrue  (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 'H', 'I' });

        Assert::IsTrue  (CassqueActions::TryParseSearch (L"ZZ", bytes, isText));
        Assert::IsTrue  (isText, L"What is not hex digits is text");
        Assert::IsTrue  (bytes == std::vector<Byte> { 'Z', 'Z' });
        Assert::IsFalse (CassqueActions::TryParseSearch (L"\"\"", bytes, isText));
        Assert::IsFalse (CassqueActions::TryParseSearch (L"", bytes, isText));
    }


    TEST_METHOD (Search_FindsForwardWrapsAndMatchesTextEitherWay)
    {
        std::vector<Byte>  haystack = { 0x01, 0xC8, 0xC9, 0x02, 0x48, 0x49 };

        //  "hi" matches high-bit "HI" at 1, then plain "HI" at 4, then wraps.
        Assert::AreEqual ((size_t) 1, CassqueActions::FindBytes (haystack, { 'h', 'i' }, true, 0));
        Assert::AreEqual ((size_t) 4, CassqueActions::FindBytes (haystack, { 'h', 'i' }, true, 2));
        Assert::AreEqual ((size_t) 1, CassqueActions::FindBytes (haystack, { 'h', 'i' }, true, 5));

        //  Bytes match exactly.
        Assert::AreEqual ((size_t) 4, CassqueActions::FindBytes (haystack, { 0x48 }, false, 0));
        Assert::AreEqual (CassqueActions::kNotFound, CassqueActions::FindBytes (haystack, { 0x77 }, false, 0));
    }


    TEST_METHOD (Search_InSourceFindsAMatchAcrossTwoChunksAndWraps)
    {
        std::vector<Byte>       haystack (CassqueActions::kSearchChunkBytes * 2 + 100, (Byte) 0);
        uint64_t                across = CassqueActions::kSearchChunkBytes - 2;
        CassqueActions::ReadFn  read   = [&haystack] (uint64_t offset, std::span<uint8_t> out)
        {
            std::copy_n (haystack.begin() + (ptrdiff_t) offset, out.size(), out.begin());
        };

        haystack[(size_t) across]     = 0xDE;
        haystack[(size_t) across + 1] = 0xAD;
        haystack[(size_t) across + 2] = 0xBE;
        haystack[(size_t) across + 3] = 0xEF;

        Assert::AreEqual (across, CassqueActions::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, 0),
            L"A match that straddles the first chunk's end is found");
        Assert::AreEqual (across, CassqueActions::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, across + 1),
            L"and a search past it wraps back to it");
        Assert::AreEqual (CassqueActions::kNotFoundOffset, CassqueActions::FindInSource (read, haystack.size(), { 0x77 }, false, 0));
    }


    TEST_METHOD (Numbers_ParseSpacesAndCommas)
    {
        std::vector<int>  numbers;

        Assert::IsTrue   (CassqueActions::TryParseNumbers (L"17 0 2", 2, numbers));
        Assert::AreEqual ((size_t) 3, numbers.size());
        Assert::AreEqual (17, numbers[0]);
        Assert::IsTrue   (CassqueActions::TryParseNumbers (L"17,0", 2, numbers));
        Assert::IsFalse  (CassqueActions::TryParseNumbers (L"17", 2, numbers));
        Assert::IsFalse  (CassqueActions::TryParseNumbers (L"1 2 3 4", 1, numbers));
        Assert::IsFalse  (CassqueActions::TryParseNumbers (L"17 x", 1, numbers));
    }


    TEST_METHOD (Sectors_RoundTripThroughAHostFile)
    {
        Host  host;

        host.OpenImage();

        Assert::IsTrue (host.actions.ReadSectors (17, 0, 1, L"C:\\Out\\vtoc.sec").Succeeded());
        Assert::IsTrue (host.io.files.count ("C:\\Out\\vtoc.sec") == 1);
        Assert::AreEqual ((size_t) 256, host.io.files["C:\\Out\\vtoc.sec"].size());

        Assert::IsTrue   (host.actions.WriteSectors (20, 5, L"C:\\Out\\vtoc.sec").Succeeded());
        Assert::AreEqual ((size_t) 1, host.written.size());
    }


    TEST_METHOD (Put_OutsideAnImage_IsRefused)
    {
        Host  host;

        host.OpenImage();
        host.browser.GoUp();

        Assert::IsFalse (host.actions.PutFiles ({ L"C:\\In\\x.txt" }).Succeeded());
    }
};
