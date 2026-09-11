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


    TEST_METHOD (Put_OutsideAnImage_IsRefused)
    {
        Host  host;

        host.OpenImage();
        host.browser.GoUp();

        Assert::IsFalse (host.actions.PutFiles ({ L"C:\\In\\x.txt" }).Succeeded());
    }
};
