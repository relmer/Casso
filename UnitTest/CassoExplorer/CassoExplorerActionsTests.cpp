#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "Core/AppleSingleCodec.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActionsTests
//
//  How a host file is planned into an image, and the menu verbs run end to
//  end over the scratch DOS 3.3 image: the catalog after the write, the
//  written callback, and the host file a get produces.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerActionsTests)
{
public:

    struct Host
    {
        InMemoryFileSystem         fs;
        FakeDiskFileIo             io;
        CassoExplorerBrowser       browser { fs, io };
        CassoExplorerActions       actions { browser, fs };
        std::vector<std::wstring>  written;

        Host()
        {
            SeedFixture ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");

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



    static bool Offers (const std::vector<CassoExplorerActions::Verb> & verbs, CassoExplorerActions::Verb verb)
    {
        return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
    }


    static std::vector<Byte> Bytes (const char * text)
    {
        return std::vector<Byte> (text, text + strlen (text));
    }



    TEST_METHOD (PlanPut_ConvertedSuffixesPickTheConversion)
    {
        CassoExplorerActions::PutPlan  plan;

        plan = CassoExplorerActions::PlanPut (L"C:\\x\\HELLO.Applesoft BASIC.txt", Bytes ("10 PRINT\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassoExplorerActions::Encoding::Basic);
        Assert::AreEqual (std::string ("HELLO"), plan.catalogName);

        plan = CassoExplorerActions::PlanPut (L"NOTES.Text.txt", Bytes ("anything"), VolumeKind::ProDos);
        Assert::IsTrue   (plan.encoding == CassoExplorerActions::Encoding::Text);
        Assert::IsFalse  (plan.usePayload);
    }


    TEST_METHOD (PlanPut_AppleSingleGoesInAsTheFileItHolds)
    {
        AppleSingleFile                file;
        std::vector<Byte>              bytes;
        CassoExplorerActions::PutPlan  pro;
        CassoExplorerActions::PutPlan  dos;

        file.data          = std::vector<Byte> (300, 0x60);
        file.realName      = "GAME.OBJ";
        file.hasProDosInfo = true;
        file.fileType      = ProDosVolume::kTypeBinary;
        file.auxType       = 0x6000;
        AppleSingleCodec::Encode (file, bytes);

        //  Named like a disk image, so no suffix rule can be what decides.
        pro = CassoExplorerActions::PlanPut (L"C:\\x\\DOWNLOAD.po", bytes, VolumeKind::ProDos);
        dos = CassoExplorerActions::PlanPut (L"C:\\x\\DOWNLOAD.po", bytes, VolumeKind::Dos33);

        Assert::IsTrue   (pro.usePayload, L"No dialog: the type is the container's");
        Assert::IsTrue   (pro.refusal.empty());
        Assert::AreEqual (std::string ("GAME.OBJ"), pro.catalogName);
        Assert::IsTrue   (pro.payload.bytes == file.data, L"The data fork is the contents");
        Assert::AreEqual ((int) ProDosVolume::kTypeBinary, (int) pro.payload.type);
        Assert::IsTrue   (pro.payload.hasAuxType);
        Assert::AreEqual ((int) 0x6000, (int) pro.payload.auxType);

        Assert::IsTrue   (dos.usePayload);
        Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) dos.payload.type, L"Mapped to DOS 3.3's binary type");
        Assert::IsTrue   (dos.payload.hasLoadAddress);
        Assert::AreEqual ((int) 0x6000, (int) dos.payload.loadAddress, L"The aux type is the load address");
        Assert::IsFalse  (dos.catalogName.empty());
    }


    TEST_METHOD (PlanPut_AppleSingleWithoutProDosInfoIsABinaryUnderItsName)
    {
        AppleSingleFile                file;
        std::vector<Byte>              bytes;
        CassoExplorerActions::PutPlan  plan;

        file.data = std::vector<Byte> (8192, 0x00);
        AppleSingleCodec::Encode (file, bytes);

        plan = CassoExplorerActions::PlanPut (L"C:\\x\\PICTURE.as", bytes, VolumeKind::ProDos);

        Assert::IsTrue   (plan.usePayload);
        Assert::AreEqual (std::string ("PICTURE"), plan.catalogName, L"With no real name, the host name without .as");
        Assert::AreEqual ((int) ProDosVolume::kTypeBinary, (int) plan.payload.type);
        Assert::AreEqual ((int) 0x2000, (int) plan.payload.auxType);
    }


    TEST_METHOD (WriteProtected_OffersReadingButNotWriting)
    {
        Host                                     host;
        std::vector<CassoExplorerActions::Verb>  verbs;
        bool                                     readOnly = false;

        AssertSucceeded (host.fs.SetReadOnlyAttribute (L"C:\\Disks\\dos33.dsk", true));
        AssertSucceeded (host.fs.GetReadOnlyAttribute (L"C:\\Disks\\dos33.dsk", readOnly));
        Assert::IsTrue  (readOnly, L"the fake file system must hold the attribute");

        host.OpenImage();
        Assert::IsTrue (host.browser.IsWriteProtected(), L"the browser reads it when the image loads");

        host.browser.SetSelectedRows ({ 0 });
        verbs = host.actions.GetListVerbs();

        Assert::IsTrue  (Offers (verbs, CassoExplorerActions::Verb::Get),    L"copying a file out reads only");
        Assert::IsFalse (Offers (verbs, CassoExplorerActions::Verb::Delete), L"delete would write");
        Assert::IsFalse (Offers (verbs, CassoExplorerActions::Verb::Rename), L"rename would write");
        Assert::IsFalse (Offers (verbs, CassoExplorerActions::Verb::Put),    L"put would write");
        Assert::IsFalse (Offers (verbs, CassoExplorerActions::Verb::Format), L"format would write");
    }


    TEST_METHOD (Writable_OffersTheWritingVerbs)
    {
        Host                                     host;
        std::vector<CassoExplorerActions::Verb>  verbs;

        host.OpenImage();
        Assert::IsFalse (host.browser.IsWriteProtected());

        host.browser.SetSelectedRows ({ 0 });
        verbs = host.actions.GetListVerbs();

        Assert::IsTrue (Offers (verbs, CassoExplorerActions::Verb::Delete));
        Assert::IsTrue (Offers (verbs, CassoExplorerActions::Verb::Rename));
        Assert::IsTrue (Offers (verbs, CassoExplorerActions::Verb::Put));
        Assert::IsTrue (Offers (verbs, CassoExplorerActions::Verb::Format));
    }


    TEST_METHOD (DescribeDrop_AFileThatRecordsItsTypeNeedsNoChoice)
    {
        Host  host;

        host.SeedBytes (std::vector<Byte> (40, 0xEA), L"C:\\Drop\\PIC#062000");
        host.SeedBytes (std::vector<Byte> { 'h', 'i' }, L"C:\\Drop\\NOTES.Text.txt");

        CassoExplorerActions::DropKind  typed     = host.actions.DescribeDrop ({ L"C:\\Drop\\PIC#062000" },   VolumeKind::ProDos);
        CassoExplorerActions::DropKind  converted = host.actions.DescribeDrop ({ L"C:\\Drop\\NOTES.Text.txt" }, VolumeKind::ProDos);

        Assert::IsTrue   (typed.determined, L"A CiderPress name says what the file is");
        Assert::AreEqual (std::wstring (L"BIN $2000"), typed.label);

        Assert::IsTrue   (converted.determined, L"A descriptive name says which conversion");
        Assert::AreEqual (std::wstring (L"text"), converted.label);
    }


    TEST_METHOD (DescribeDrop_AFileThatRecordsNothingLeavesTheChoiceOpen)
    {
        Host  host;

        //  A binary with no suffix: only the content rule could decide, and
        //  that is a guess, so the menu must offer the conversions.
        host.SeedBytes (std::vector<Byte> (64, 0x80), L"C:\\Drop\\download");
        host.SeedBytes (std::vector<Byte> (40, 0xEA), L"C:\\Drop\\PIC#062000");

        Assert::IsFalse (host.actions.DescribeDrop ({ L"C:\\Drop\\download" }, VolumeKind::ProDos).determined);

        //  Two files that describe themselves differently agree on nothing.
        Assert::IsFalse (host.actions.DescribeDrop ({ L"C:\\Drop\\download", L"C:\\Drop\\PIC#062000" }, VolumeKind::ProDos).determined);
    }


    TEST_METHOD (PutInto_ForcedConversion_OverridesTheContentRule)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        VolumeListing                  listing;
        VolumeKind                     kind    = VolumeKind::Unknown;
        bool                           asText  = false;

        host.OpenImage();

        //  Printable bytes, which the content rule would put as text; forced
        //  to binary they go down raw at the address the sniffer suggests.
        host.SeedBytes (std::vector<Byte> (300, 0x41), L"C:\\Drop\\PLAIN");

        outcome = host.actions.PutInto (L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, std::string(), { L"C:\\Drop\\PLAIN" },
                                        [] (const std::wstring &, Word, Word & out) { out = 0x0803; return true; },
                                        CassoExplorerActions::Conversion::Binary);

        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());

        AssertSucceeded (host.browser.GetOperations().List ("C:\\Disks\\dos33.dsk", listing, kind).hr, L"List");

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == "PLAIN")
            {
                asText = entry.type == Dos33Volume::kTypeText;
                Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) entry.type, L"Forced binary, not the content rule's text");
            }
        }

        Assert::IsFalse (asText);
    }


    TEST_METHOD (PutInto_ACanceledAddressPrompt_IsNotWhole)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;

        host.OpenImage();
        host.SeedBytes (std::vector<Byte> (300, 0x41), L"C:\\Drop\\PLAIN");

        //  A move takes away only what a whole put wrote, so a file skipped at
        //  its prompt stays where it is.
        outcome = host.actions.PutInto (L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, std::string(), { L"C:\\Drop\\PLAIN" },
                                        [] (const std::wstring &, Word, Word &) { return false; },
                                        CassoExplorerActions::Conversion::Binary);

        Assert::IsTrue  (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (0, outcome.written);
        Assert::IsFalse (outcome.IsWhole(), L"a canceled prompt skips the file");

        outcome = host.actions.PutInto (L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, std::string(), { L"C:\\Drop\\PLAIN" },
                                        [] (const std::wstring &, Word, Word & out) { out = 0x0803; return true; },
                                        CassoExplorerActions::Conversion::Binary);

        Assert::IsTrue (outcome.IsWhole(), outcome.message.c_str());
    }


    TEST_METHOD (PlanPut_CiderPressSuffixWritesATypedPayload)
    {
        std::vector<Byte>        bytes (100, 0xEA);
        CassoExplorerActions::PutPlan  pro = CassoExplorerActions::PlanPut (L"PIC#062000", bytes, VolumeKind::ProDos);
        CassoExplorerActions::PutPlan  dos = CassoExplorerActions::PlanPut (L"PIC#062000", bytes, VolumeKind::Dos33);

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
        CassoExplorerActions::PutPlan  plan;

        plan = CassoExplorerActions::PlanPut (L"prog.bas", Bytes ("10 PRINT \"HI\"\n20 END\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassoExplorerActions::Encoding::Basic);
        Assert::AreEqual (std::string ("PROG"), plan.catalogName);

        plan = CassoExplorerActions::PlanPut (L"readme.md", Bytes ("Just words.\n"), VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassoExplorerActions::Encoding::Text);

        plan = CassoExplorerActions::PlanPut (L"screen.pic", picture, VolumeKind::Dos33);
        Assert::IsTrue   (plan.encoding == CassoExplorerActions::Encoding::Verbatim);
        Assert::IsTrue   (plan.guessedAddress);
        Assert::AreEqual ((int) 0x2000, (int) plan.loadAddress);
    }


    TEST_METHOD (CatalogName_FollowsEachFileSystemsRules)
    {
        Assert::AreEqual (std::string ("A1ST.FILE"),       CassoExplorerActions::MakeCatalogName (L"1st file.txt", VolumeKind::ProDos));
        Assert::AreEqual (std::string ("MY FILE.V2"),      CassoExplorerActions::MakeCatalogName (L"my file,v2.txt", VolumeKind::Dos33));
        Assert::AreEqual (std::string ("ABCDEFGHIJKLMNO"), CassoExplorerActions::MakeCatalogName (L"abcdefghijklmnopqrs.bin", VolumeKind::ProDos));
    }


    TEST_METHOD (Verbs_DependOnWhatIsSelected)
    {
        Host                                     host;
        std::vector<CassoExplorerActions::Verb>  verbs;

        host.OpenImage();
        host.browser.GoUp();

        host.browser.SetSelectedRows ({ host.Find (L"dos33.dsk") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::InsertDrive1) != verbs.end());

        host.browser.OpenRow (host.Find (L"dos33.dsk"));
        host.browser.SetSelectedRows ({ host.Find (L"HELLO") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Rename) != verbs.end());
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::InsertDrive1) != verbs.end());
        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Open) != verbs.end(),
                         L"A file in an image opens in its own program, as one in a zip does");
    }


    TEST_METHOD (AHostFile_OpensInItsOwnProgram_AndEveryHostItemHasTheShellsMenu)
    {
        Host                                     host;
        std::vector<CassoExplorerActions::Verb>  verbs;
        auto                               has = [&verbs] (CassoExplorerActions::Verb verb) { return std::find (verbs.begin(), verbs.end(), verb) != verbs.end(); };

        host.SeedBytes (Bytes ("hello"), L"C:\\Disks\\readme.txt");
        host.OpenImage();
        host.browser.GoUp();

        host.browser.SetSelectedRows ({ host.Find (L"readme.txt") });
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue (has (CassoExplorerActions::Verb::Open));
        Assert::IsTrue (has (CassoExplorerActions::Verb::OpenWith));
        Assert::IsTrue (has (CassoExplorerActions::Verb::MoreOptions));
        Assert::IsTrue (has (CassoExplorerActions::Verb::Cut));
        Assert::IsTrue (has (CassoExplorerActions::Verb::Copy));
        Assert::IsTrue (has (CassoExplorerActions::Verb::Delete));
        Assert::IsTrue (has (CassoExplorerActions::Verb::Rename));
        Assert::IsFalse (has (CassoExplorerActions::Verb::Paste), L"A file is not somewhere to paste");
        Assert::IsTrue  (has (CassoExplorerActions::Verb::Share));

        //  The folder's background takes a paste.
        host.browser.SetSelectedRows ({});
        verbs = host.actions.GetListVerbs();
        Assert::IsTrue  (has (CassoExplorerActions::Verb::Paste));
        Assert::IsFalse (has (CassoExplorerActions::Verb::Cut));

        //  An image opens here, so it gets no program to open it with.
        host.browser.SetSelectedRows ({ host.Find (L"dos33.dsk") });
        verbs = host.actions.GetListVerbs();
        Assert::IsFalse (has (CassoExplorerActions::Verb::OpenWith));
        Assert::IsTrue  (has (CassoExplorerActions::Verb::MoreOptions));

        //  Nothing inside an image is a real file.
        host.browser.OpenRow (host.Find (L"dos33.dsk"));
        host.browser.SetSelectedRows ({ host.Find (L"HELLO") });
        verbs = host.actions.GetListVerbs();
        Assert::IsFalse (has (CassoExplorerActions::Verb::OpenWith));
        Assert::IsFalse (has (CassoExplorerActions::Verb::MoreOptions));
        Assert::IsFalse (has (CassoExplorerActions::Verb::Cut), L"Entries in an image are copied out with Get");
    }


    TEST_METHOD (Delete_RemovesTheEntryAndReportsTheWrite)
    {
        Host  host;

        host.OpenImage();
        host.browser.SetSelectedRows ({ host.Find (L"NOTES") });

        CassoExplorerActions::Outcome  outcome = host.actions.DeleteSelected();

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

        CassoExplorerActions::Outcome  outcome = host.actions.GetSelected (L"C:\\Out", HostFileNaming::Style::Descriptive);

        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual (2, outcome.written);
        Assert::IsTrue   (host.io.files.count ("C:\\Out\\HELLO.Applesoft BASIC.txt") == 1);
        Assert::IsTrue   (host.written.empty());
    }


    //  A file of the same name in the folder is not written over, and the
    //  copy is numbered ahead of the suffix with its type.
    TEST_METHOD (Get_KeepsAFileThatIsThereAlready)
    {
        Host  host;

        host.OpenImage();
        AssertSucceeded (host.fs.MakeDirectory (L"C:\\Out"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Out\\HELLO.Applesoft BASIC.txt", "mine"));
        host.browser.SetSelectedRows ({ host.Find (L"HELLO") });

        CassoExplorerActions::Outcome  outcome = host.actions.GetSelected (L"C:\\Out", HostFileNaming::Style::Descriptive);

        Assert::IsTrue (outcome.Succeeded());
        Assert::IsTrue (host.io.files.count ("C:\\Out\\HELLO (2).Applesoft BASIC.txt") == 1);
        Assert::IsTrue (host.io.files.count ("C:\\Out\\HELLO.Applesoft BASIC.txt") == 0, L"the file there already is not written over");
    }


    //  Each choice the collision prompt returns: the file there already is
    //  written over, left with nothing copied, or kept beside a numbered copy.
    TEST_METHOD (Get_FollowsTheCollisionChoice)
    {
        using Collision = CassoExplorerActions::Collision;

        const wchar_t  * kMine  = L"C:\\Out\\HELLO.Applesoft BASIC.txt";
        const char     * kMineN = "C:\\Out\\HELLO.Applesoft BASIC.txt";

        for (Collision choice : { Collision::Replace, Collision::Skip, Collision::KeepBoth })
        {
            Host                           host;
            CassoExplorerActions::Outcome  outcome;
            int                            asked = 0;

            host.OpenImage();
            AssertSucceeded (host.fs.MakeDirectory (L"C:\\Out"));
            AssertSucceeded (host.fs.WriteAllText (kMine, "mine"));
            host.browser.SetSelectedRows ({ host.Find (L"HELLO") });

            outcome = host.actions.GetSelected (L"C:\\Out", HostFileNaming::Style::Descriptive,
                                                [&asked, choice] (const std::wstring &, int index, int count)
                                                {
                                                    asked++;
                                                    Assert::AreEqual (1, index);
                                                    Assert::AreEqual (1, count);
                                                    return choice;
                                                });

            Assert::IsTrue   (outcome.Succeeded());
            Assert::AreEqual (1, asked, L"asked once, for the one file there already");
            Assert::AreEqual (choice == Collision::Replace,  host.io.files.count (kMineN) == 1, L"written over only on Replace");
            Assert::AreEqual (choice == Collision::KeepBoth, host.io.files.count ("C:\\Out\\HELLO (2).Applesoft BASIC.txt") == 1, L"numbered only on Keep both");
        }
    }


    //  Casso Explorer's own copy on the clipboard: two files, the second of
    //  whose contents cannot be read when `failSecond` is set.
    static DxuiDragDropSource * MakeCopy (bool failSecond, bool ours = true)
    {
        std::vector<DxuiDragDropSource::Format>  formats;
        DxuiDragDropSource                     * source = nullptr;

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), 1, [] (int, std::vector<uint8_t> & out)
        {
            const wchar_t  * names[] = { L"HELLO.Applesoft BASIC.txt", L"MUFFIN.Binary.bin" };
            FILEDESCRIPTORW  items[2] = {};
            UINT             count    = 2;

            for (int i = 0; i < 2; i++)
            {
                items[i].dwFlags      = FD_FILESIZE;
                items[i].nFileSizeLow = 5;
                wcscpy_s (items[i].cFileName, names[i]);
            }

            out.assign (sizeof (UINT) + sizeof (items), 0);
            memcpy (out.data(), &count, sizeof (count));
            memcpy (out.data() + offsetof (FILEGROUPDESCRIPTORW, fgd), items, sizeof (items));
            return S_OK;
        } });

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), 2, [failSecond] (int index, std::vector<uint8_t> & out)
        {
            std::string  text = (index == 0) ? "hello" : "bytes";

            out.assign (text.begin(), text.end());
            return (failSecond && index == 1) ? HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT) : S_OK;
        } });

        if (ours)
        {
            formats.push_back (CassoExplorerDragOut::MakeFilesMarker());
        }

        AssertSucceeded (DxuiDragDropSource::Create (std::move (formats), &source));
        return source;
    }


    //  A paste of an image's files into a host folder asks about a name in
    //  use as Get does, Keep both included, rather than leaving it to the
    //  shell's prompt, which offers no Keep both.
    TEST_METHOD (PasteVirtualFiles_FollowsTheCollisionChoice)
    {
        using Collision = CassoExplorerActions::Collision;

        const wchar_t  * kMine  = L"C:\\Out\\HELLO.Applesoft BASIC.txt";

        for (Collision choice : { Collision::Replace, Collision::Skip, Collision::KeepBoth })
        {
            Host                           host;
            CassoExplorerActions::Outcome  outcome;
            DxuiDragDropSource           * copy    = MakeCopy (false);
            std::string                    text;
            std::vector<std::wstring>      created;
            int                            asked   = 0;

            AssertSucceeded (host.fs.MakeDirectory (L"C:\\Out"));
            AssertSucceeded (host.fs.WriteAllText (kMine, "mine"));

            outcome = host.actions.PasteVirtualFiles (copy, L"C:\\Out", [&asked, choice] (const std::wstring &, int, int)
                                                      {
                                                          asked++;
                                                          return choice;
                                                      }, created);

            Assert::IsTrue   (outcome.Succeeded());
            Assert::AreEqual (1, asked, L"asked once, for the one file there already");
            AssertSucceeded  (host.fs.ReadAllText (kMine, text));
            Assert::IsTrue   (host.fs.Exists (L"C:\\Out\\MUFFIN.Binary.bin"), L"the other file is written");
            Assert::AreEqual (std::string (choice == Collision::Replace ? "hello" : "mine"), text, L"written over only on Replace");
            Assert::AreEqual (choice == Collision::KeepBoth, host.fs.Exists (L"C:\\Out\\HELLO (2).Applesoft BASIC.txt"), L"numbered only on Keep both");
            Assert::AreEqual ((size_t) (choice == Collision::Skip ? 1 : 2), created.size(), L"each file written, for the undo");

            copy->Release();
        }
    }


    //  A file whose contents cannot be read is named in the message, and the
    //  others are still written; only Casso Explorer's own copy is taken.
    TEST_METHOD (PasteVirtualFiles_NamesAFileThatCannotBeRead)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        DxuiDragDropSource           * copy    = MakeCopy (true);
        DxuiDragDropSource           * foreign = MakeCopy (false, false);
        std::vector<std::wstring>      created;

        AssertSucceeded (host.fs.MakeDirectory (L"C:\\Out"));

        outcome = host.actions.PasteVirtualFiles (copy, L"C:\\Out", nullptr, created);

        Assert::IsFalse (outcome.Succeeded());
        Assert::IsTrue  (outcome.message.starts_with (L"Unable to copy MUFFIN.Binary.bin: "), outcome.message.c_str());
        Assert::IsTrue  (host.fs.Exists (L"C:\\Out\\HELLO.Applesoft BASIC.txt"));
        Assert::AreEqual ((size_t) 1, created.size());

        Assert::IsTrue  (ActionsCopy::IsCassoCopy (copy));
        Assert::IsFalse (ActionsCopy::IsCassoCopy (foreign), L"another program's files go to the shell");

        copy->Release();
        foreign->Release();
    }


    TEST_METHOD (ExtractImage_CopiesEveryFileIntoAFolderBesideTheImage)
    {
        Host          host;
        std::wstring  folder;
        std::wstring  second;

        CassoExplorerActions::Outcome  outcome = host.actions.ExtractImage (L"C:\\Disks\\dos33.dsk", HostFileNaming::Style::Descriptive, folder);
        std::string                    listing;

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33"), folder);
        Assert::IsTrue   (host.fs.DirectoryExists (folder));
        Assert::IsTrue   (host.io.files.count ("C:\\Disks\\dos33\\HELLO.Applesoft BASIC.txt") == 1);
        Assert::IsTrue   (host.io.files.count ("C:\\Disks\\dos33\\ODD.Binary.$0803.bin") == 1);
        Assert::IsTrue   (host.io.files.count ("C:\\Disks\\dos33\\INTPROG.Integer BASIC.txt") == 1);

        //  The Integer BASIC program comes out as its listing.
        listing.assign (host.io.files["C:\\Disks\\dos33\\INTPROG.Integer BASIC.txt"].begin(), host.io.files["C:\\Disks\\dos33\\INTPROG.Integer BASIC.txt"].end());
        Assert::IsTrue   (listing.find ("PRINT") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());

        //  A second extraction goes into a folder of its own.
        outcome = host.actions.ExtractImage (L"C:\\Disks\\dos33.dsk", HostFileNaming::Style::Descriptive, second);

        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33 (2)"), second);
        Assert::IsTrue   (host.io.files.count ("C:\\Disks\\dos33 (2)\\HELLO.Applesoft BASIC.txt") == 1);
    }


    TEST_METHOD (ExtractImage_MakesAFolderForEachProDosDirectory)
    {
        Host          host;
        std::wstring  folder;
        size_t        inside = 0;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        host.SeedBytes (Bytes ("inner text\n"), L"C:\\In\\inner.txt");
        Assert::IsTrue (host.browser.GetOperations().Put ("C:\\Disks\\prodos.po", "C:\\In\\inner.txt", "SUBDIR/INNER", "T", false, 0, DiskOperations::Encoding::Text).Succeeded());

        CassoExplorerActions::Outcome  outcome = host.actions.ExtractImage (L"C:\\Disks\\prodos.po", HostFileNaming::Style::Descriptive, folder);

        for (const auto & [path, bytes] : host.io.files)
        {
            inside += path.starts_with ("C:\\Disks\\prodos\\SUBDIR\\") ? 1 : 0;
        }

        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());
        Assert::IsTrue (host.fs.DirectoryExists (L"C:\\Disks\\prodos\\SUBDIR"));
        Assert::IsTrue (inside > 0, L"the directory's files are inside its folder");
    }


    TEST_METHOD (Put_AddsHostFilesByTheirContent)
    {
        Host  host;

        host.SeedBytes (Bytes ("10 PRINT \"PUT\"\n20 END\n"), L"C:\\In\\putprog.bas");
        host.OpenImage();

        CassoExplorerActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\putprog.bas" });

        Assert::IsTrue      (outcome.Succeeded());
        Assert::AreNotEqual (-1, host.Find (L"PUTPROG"));
        Assert::AreEqual    ((size_t) 1, host.written.size());
    }


    TEST_METHOD (InsideAProDosSubdirectory_FilesArePutRenamedAndDeleted)
    {
        Host                                     host;
        std::vector<CassoExplorerActions::Verb>  verbs;
        std::vector<DxuiTreeNode>                roots;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
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

        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Delete) != verbs.end());
        Assert::IsTrue  (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Rename) != verbs.end());
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Boot)   != verbs.end(), L"Nothing boots from a subdirectory");
        Assert::IsFalse (std::find (verbs.begin(), verbs.end(), CassoExplorerActions::Verb::Format) != verbs.end(), L"A subdirectory is not the disk");

        Assert::IsTrue      (host.actions.RenameSelected (L"moved").Succeeded());
        Assert::AreEqual    (-1, host.Find (L"SUBPROG"));
        Assert::AreNotEqual (-1, host.Find (L"MOVED"));

        host.browser.SetSelectedRows ({ host.Find (L"MOVED") });
        Assert::IsTrue   (host.actions.DeleteSelected().Succeeded());
        Assert::AreEqual (-1, host.Find (L"MOVED"));
    }


    TEST_METHOD (ANewItemsName_IsTheFirstOneNotInUse)
    {
        Assert::AreEqual (std::wstring (L"New folder"),     CassoExplorerActions::MakeUnusedName (L"New folder", { L"a.txt" }, true));
        Assert::AreEqual (std::wstring (L"New folder (2)"), CassoExplorerActions::MakeUnusedName (L"New folder", { L"new FOLDER" }, true),
                          L"Case does not tell names apart");
        Assert::AreEqual (std::wstring (L"New folder (3)"), CassoExplorerActions::MakeUnusedName (L"New folder", { L"New folder", L"New folder (2)" }, true));

        Assert::AreEqual (std::wstring (L"NEW.FOLDER.2"),   CassoExplorerActions::MakeUnusedName (L"NEW.FOLDER", { L"NEW.FOLDER" }, false));
        Assert::AreEqual (std::wstring (L"ABCDEFGHIJKLM.2"), CassoExplorerActions::MakeUnusedName (L"ABCDEFGHIJKLMNO", { L"ABCDEFGHIJKLMNO" }, false),
                          L"Within ProDOS's fifteen characters");
    }


    TEST_METHOD (SpaceIsCountedInBlocksAndSectors)
    {
        Assert::AreEqual ((uint64_t) 1,   ActionsCopy::CountUnitsForFile (VolumeKind::ProDos, 0),      L"A seedling file");
        Assert::AreEqual ((uint64_t) 1,   ActionsCopy::CountUnitsForFile (VolumeKind::ProDos, 512));
        Assert::AreEqual ((uint64_t) 3,   ActionsCopy::CountUnitsForFile (VolumeKind::ProDos, 513),    L"Two data blocks and an index block");
        Assert::AreEqual ((uint64_t) 2,   ActionsCopy::CountUnitsForFile (VolumeKind::Dos33, 100),     L"A sector and its track/sector list");
        Assert::AreEqual ((uint64_t) 125, ActionsCopy::CountUnitsForFile (VolumeKind::Dos33, 123 * 256), L"123 sectors need a second list");
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
        Host                           host;
        CassoExplorerActions::Outcome  outcome;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
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


    //  An undo takes out what a put made: the folder alone, since its file
    //  goes with it, and the entry back out of the image.
    TEST_METHOD (APut_ReportsTheEntriesItMade_AndTheyDeleteAgain)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        CassoExplorerActions::Outcome  undone;
        VolumeListing                  listing;
        VolumeKind                     kind = VolumeKind::Unknown;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        host.SeedBytes   (Bytes ("10 PRINT \"IN\"\n20 END\n"), L"C:\\In\\Games\\inner.bas");

        outcome = host.actions.PutInto (L"C:\\Disks\\prodos.po", VolumeKind::ProDos, "", { L"C:\\In\\Games" });
        Assert::IsTrue   (outcome.Succeeded());
        Assert::AreEqual ((size_t) 1, outcome.created.size(), L"The directory made, not the file inside it");
        Assert::AreEqual (std::string ("GAMES"), outcome.created[0]);

        undone = host.actions.DeleteEntries ("C:\\Disks\\prodos.po", outcome.created);
        Assert::IsTrue  (undone.Succeeded());
        Assert::IsFalse (host.browser.GetOperations().List ("C:\\Disks\\prodos.po", "GAMES", listing, kind).Succeeded(), L"and gone again");
    }


    TEST_METHOD (EntriesDraggedBetweenImages_KeepTheirTypeAcrossFileSystems)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        VolumeListing                  listing;
        VolumeKind                     kind    = VolumeKind::Unknown;
        FilePayload                    payload;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");

        //  An Applesoft program from DOS 3.3 lands as a ProDOS BAS file.
        outcome = host.actions.CopyEntriesInto ("C:\\Disks\\dos33.dsk", VolumeKind::Dos33, { "HELLO" },
                                                L"C:\\Disks\\prodos.po", VolumeKind::ProDos, "");
        Assert::IsTrue (outcome.Succeeded());
        Assert::IsTrue (host.browser.GetOperations().Read ("C:\\Disks\\prodos.po", "HELLO", payload).Succeeded());
        Assert::AreEqual ((int) 0xFC, (int) payload.type, L"BAS");

        //  A ProDOS directory has nowhere to go on DOS 3.3.
        Assert::IsTrue (host.browser.GetOperations().Mkdir ("C:\\Disks\\prodos.po", "SUBDIR2").Succeeded());
        outcome = host.actions.CopyEntriesInto ("C:\\Disks\\prodos.po", VolumeKind::ProDos, { "SUBDIR2" },
                                                L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, "");
        Assert::IsFalse (outcome.Succeeded());

        //  And into another ProDOS directory it goes with its contents.
        outcome = host.actions.CopyEntriesInto ("C:\\Disks\\prodos.po", VolumeKind::ProDos, { "HELLO" },
                                                L"C:\\Disks\\prodos.po", VolumeKind::ProDos, "SUBDIR2");
        Assert::IsTrue (outcome.Succeeded());
        Assert::IsTrue (host.browser.GetOperations().List ("C:\\Disks\\prodos.po", "SUBDIR2", listing, kind).Succeeded());
        Assert::AreEqual ((size_t) 1, listing.entries.size());
    }


    //  A copy to another disk is locked, and on ProDOS dated, as its
    //  original is.
    TEST_METHOD (ACopyBetweenImages_KeepsTheLockAndTheDates)
    {
        constexpr Word  kDate = 0x5A21;
        constexpr Word  kTime = 0x0C1E;

        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        FilePayload                    locked;
        FilePayload                    copied;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");

        locked.bytes        = { 1, 2, 3 };
        locked.type         = 0x04;
        locked.access       = FilePayload::kAccessLocked;
        locked.hasAccess    = true;
        locked.createdDate  = kDate;
        locked.createdTime  = kTime;
        locked.modifiedDate = kDate;
        locked.modifiedTime = kTime;
        locked.hasDates     = true;
        Assert::IsTrue (host.browser.GetOperations().WritePayload ("C:\\Disks\\prodos.po", "LOCKED", locked).Succeeded(), L"the locked file is written");

        outcome = host.actions.CopyEntriesInto ("C:\\Disks\\prodos.po", VolumeKind::ProDos, { "LOCKED" },
                                                L"C:\\Disks\\dos33.dsk", VolumeKind::Dos33, "");
        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());
        Assert::IsTrue (host.browser.GetOperations().Read ("C:\\Disks\\dos33.dsk", "LOCKED", copied).Succeeded(), L"and read from DOS 3.3");
        Assert::IsTrue (copied.IsLocked(), L"locked on DOS 3.3");

        Assert::IsTrue (host.browser.GetOperations().Mkdir ("C:\\Disks\\prodos.po", "SUB").Succeeded());
        outcome = host.actions.CopyEntriesInto ("C:\\Disks\\prodos.po", VolumeKind::ProDos, { "LOCKED" },
                                                L"C:\\Disks\\prodos.po", VolumeKind::ProDos, "SUB");
        Assert::IsTrue   (outcome.Succeeded());
        Assert::IsTrue   (host.browser.GetOperations().Read ("C:\\Disks\\prodos.po", "SUB/LOCKED", copied).Succeeded());
        Assert::AreEqual ((int) FilePayload::kAccessLocked, (int) copied.access, L"locked on ProDOS");
        Assert::AreEqual ((int) kDate, (int) copied.modifiedDate);
        Assert::AreEqual ((int) kTime, (int) copied.createdTime);
    }


    //  A folder is not copied into itself or into a folder inside it, as a
    //  paste there would otherwise copy its copies.
    TEST_METHOD (AFolderIsNotCopiedIntoItself)
    {
        Host                           host;
        CassoExplorerActions::Outcome  outcome;
        VolumeListing                  listing;
        VolumeKind                     kind    = VolumeKind::Unknown;

        host.SeedFixture ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        Assert::IsTrue (host.browser.GetOperations().Mkdir ("C:\\Disks\\prodos.po", "OUTER").Succeeded());
        Assert::IsTrue (host.browser.GetOperations().Mkdir ("C:\\Disks\\prodos.po", "OUTER/INNER").Succeeded());

        for (const char * into : { "OUTER", "outer/INNER" })
        {
            outcome = host.actions.CopyEntriesInto ("C:\\Disks\\prodos.po", VolumeKind::ProDos, { "OUTER" },
                                                    L"C:\\Disks\\prodos.po", VolumeKind::ProDos, into);
            Assert::IsFalse (outcome.Succeeded(), L"the copy fails");
        }

        Assert::IsTrue   (host.browser.GetOperations().List ("C:\\Disks\\prodos.po", "OUTER/INNER", listing, kind).Succeeded());
        Assert::IsTrue   (listing.entries.empty(), L"and nothing was copied");
        Assert::IsTrue   (ActionsCopy::IsInsideFolder ("A/B", "a"));
        Assert::IsFalse  (ActionsCopy::IsInsideFolder ("AB", "A"), L"a name that starts the same is not inside");
    }


    TEST_METHOD (NewDiskChoices_MapToTheRunnersNames)
    {
        DiskOperations::NewDiskRequest  request = CassoExplorerNewDiskChoices::MakeRequest (1, 2, L"MYVOL", true);

        Assert::AreEqual (std::string ("prodos"), request.formatName);
        Assert::AreEqual (std::string ("po"),     request.containerType);
        Assert::AreEqual (std::string ("MYVOL"),  request.volumeName);
        Assert::IsTrue   (request.bootable);

        request = CassoExplorerNewDiskChoices::MakeRequest (2, 99, L"", true);
        Assert::AreEqual (std::string ("none"), request.formatName);
        Assert::AreEqual (std::string ("woz"),  request.containerType);
        Assert::IsFalse  (request.bootable);

        Assert::AreEqual (std::wstring (L"Games.dsk"), CassoExplorerNewDiskChoices::ApplyExtension (L"Games", 1));
        Assert::AreEqual (std::wstring (L"Games.po"),  CassoExplorerNewDiskChoices::ApplyExtension (L"Games.po", 1));
    }


    //  An extension typed on the name picks the container, and the format
    //  where the extension implies one.
    TEST_METHOD (NewDiskChoices_TypedExtensionPicksContainerAndFormat)
    {
        int  container = 99;
        int  format    = 99;

        Assert::IsTrue (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games.po", container, format));
        Assert::AreEqual (2, container);
        Assert::AreEqual (CassoExplorerNewDiskChoices::kFormatProDos, format);

        Assert::IsTrue (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games.DO", container, format));
        Assert::AreEqual (1, container);
        Assert::AreEqual (CassoExplorerNewDiskChoices::kFormatDos33, format);

        Assert::IsTrue (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games.dsk", container, format));
        Assert::AreEqual (1, container);
        Assert::AreEqual (CassoExplorerNewDiskChoices::kFormatDos33, format);

        Assert::IsTrue (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games.woz", container, format));
        Assert::AreEqual (0, container);
        Assert::AreEqual (-1, format);

        container = 7;
        format    = 7;
        Assert::IsFalse (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games", container, format));
        Assert::IsFalse (CassoExplorerNewDiskChoices::GetChoicesForExtension (L"Games.txt", container, format));
        Assert::AreEqual (7, container);
        Assert::AreEqual (7, format);
    }


    //  The new-disk dialog checks its fields as they are typed: a file name the
    //  host can hold and that is not taken, and a volume the format can use.
    TEST_METHOD (NewDiskChoices_ValidateFieldsAsTyped)
    {
        auto  noneTaken = [] (const std::wstring &) { return false; };
        auto  taken     = [] (const std::wstring & name) { return name == L"Games.dsk"; };

        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateFileName (L"Games", 1, noneTaken).empty());
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateFileName (L"",      1, noneTaken).empty(), L"A name is required");
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateFileName (L"a:b",   1, noneTaken).empty(), L"A name the host cannot hold is refused");
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateFileName (L"Games", 1, taken).empty(),     L"A taken name is refused, extension added");
        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateFileName (L"Games", 2, taken).empty(),     L"The same name as another image type is free");

        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatDos33, L"").empty(), L"Blank takes the default");
        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatDos33, L"254").empty());
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatDos33, L"0").empty());
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatDos33, L"255").empty());
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatDos33, L"12A").empty());

        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatProDos, L"Games.2").empty(), L"Case does not matter");
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatProDos, L"2GAMES").empty(), L"A name starts with a letter");
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatProDos, L"MY GAMES").empty());
        Assert::IsFalse (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatProDos, L"ABCDEFGHIJKLMNOP").empty(), L"At most 15 characters");

        Assert::IsTrue  (CassoExplorerNewDiskChoices::ValidateVolume (CassoExplorerNewDiskChoices::kFormatNone, L"anything").empty(), L"An unformatted image has no volume to check");

        Assert::AreEqual ((size_t) 3,  CassoExplorerNewDiskChoices::GetVolumeMaxLength (CassoExplorerNewDiskChoices::kFormatDos33));
        Assert::AreEqual ((size_t) 15, CassoExplorerNewDiskChoices::GetVolumeMaxLength (CassoExplorerNewDiskChoices::kFormatProDos));
    }


    TEST_METHOD (CreateImage_WritesANewDiskAndRefusesAnExistingName)
    {
        Host                            host;
        DiskOperations::NewDiskRequest  request = CassoExplorerNewDiskChoices::MakeRequest (0, 1, L"", false);

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

        Assert::IsTrue   (host.actions.FormatImage (CassoExplorerNewDiskChoices::MakeRequest (0, 1, L"", false)).Succeeded());
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

        CassoExplorerActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\routine.obj" },
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

        CassoExplorerActions::Outcome  outcome = host.actions.PutFiles ({ L"C:\\In\\skip.obj" },
            [] (const std::wstring &, Word, Word &) { return false; });

        Assert::AreEqual (0, outcome.written);
        Assert::AreEqual (-1, host.Find (L"SKIP"));
    }


    TEST_METHOD (Address_ParsesTheUsualForms)
    {
        Word  address = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L"$2000", address));
        Assert::AreEqual ((int) 0x2000, (int) address);
        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L" 0x0803 ", address));
        Assert::AreEqual ((int) 0x0803, (int) address);
        Assert::IsTrue   (ActionsGoTo::TryParseAddress (L"#768", address));
        Assert::AreEqual ((int) 768, (int) address);
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"$10000", address));
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"zz", address));
        Assert::IsFalse  (ActionsGoTo::TryParseAddress (L"", address));
    }


    TEST_METHOD (GoTo_ParsesAddressesAndOffsetsFromTheCaret)
    {
        int64_t  address = 0;
        int64_t  last    = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$08FF", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08FF, address);
        Assert::AreEqual (address, last, L"One address is a range of one");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+10", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x0810, address, L"An offset is hex, as addresses are");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+$A0", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x08A0, address);
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L" -#16 ", 0x0800, address, last));
        Assert::AreEqual ((int64_t) 0x07F0, address, L"and # marks decimal");
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"+", 0x0800, address, last));
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"", 0x0800, address, last));
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"zz", 0x0800, address, last));
    }


    TEST_METHOD (GoTo_ReachesPastSixteenBitsInALargeFile)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"10000", 0, first, last));
        Assert::AreEqual ((int64_t) 0x10000, first, L"An offset is not an Apple II address and is not capped at $FFFF");
        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"+#1000000", 0x10, first, last));
        Assert::AreEqual ((int64_t) 1000016, first);
    }


    TEST_METHOD (GoTo_ParsesARangeByLastAddressOrByLength)
    {
        int64_t  first = 0;
        int64_t  last  = 0;

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$0803-$0810", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0803, first);
        Assert::AreEqual ((int64_t) 0x0810, last, L"A hyphen gives the last address");

        Assert::IsTrue   (ActionsGoTo::TryParseGoTo (L"$0803,+10", 0x0800, first, last));
        Assert::AreEqual ((int64_t) 0x0812, last, L"A comma and a plus give a length of sixteen");

        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"$0810-$0803", 0x0800, first, last),
            L"A range cannot end before it starts");
        Assert::IsFalse  (ActionsGoTo::TryParseGoTo (L"$0803,+0", 0x0800, first, last),
            L"or be empty");
    }


    TEST_METHOD (Search_ParsesHexBytesAndQuotedText)
    {
        std::vector<Byte>  bytes;
        bool               isText = false;

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L" A9 00 8d ", bytes, isText));
        Assert::IsFalse (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 0xA9, 0x00, 0x8D });

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L"\"HI\"", bytes, isText));
        Assert::IsTrue  (isText);
        Assert::IsTrue  (bytes == std::vector<Byte> { 'H', 'I' });

        Assert::IsTrue  (ActionsGoTo::TryParseSearch (L"ZZ", bytes, isText));
        Assert::IsTrue  (isText, L"What is not hex digits is text");
        Assert::IsTrue  (bytes == std::vector<Byte> { 'Z', 'Z' });
        Assert::IsFalse (ActionsGoTo::TryParseSearch (L"\"\"", bytes, isText));
        Assert::IsFalse (ActionsGoTo::TryParseSearch (L"", bytes, isText));
    }


    TEST_METHOD (Search_FindsForwardWrapsAndMatchesTextEitherWay)
    {
        std::vector<Byte>  haystack = { 0x01, 0xC8, 0xC9, 0x02, 0x48, 0x49 };

        //  "hi" matches high-bit "HI" at 1, then plain "HI" at 4, then wraps.
        Assert::AreEqual ((size_t) 1, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 0));
        Assert::AreEqual ((size_t) 4, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 2));
        Assert::AreEqual ((size_t) 1, ActionsGoTo::FindBytes (haystack, { 'h', 'i' }, true, 5));

        //  Bytes match exactly.
        Assert::AreEqual ((size_t) 4, ActionsGoTo::FindBytes (haystack, { 0x48 }, false, 0));
        Assert::AreEqual (CassoExplorerActions::kNotFound, ActionsGoTo::FindBytes (haystack, { 0x77 }, false, 0));
    }


    TEST_METHOD (Search_InSourceFindsAMatchAcrossTwoChunksAndWraps)
    {
        std::vector<Byte>       haystack (CassoExplorerActions::kSearchChunkBytes * 2 + 100, (Byte) 0);
        uint64_t                across = CassoExplorerActions::kSearchChunkBytes - 2;
        CassoExplorerActions::ReadFn  read   = [&haystack] (uint64_t offset, std::span<uint8_t> out)
        {
            std::copy_n (haystack.begin() + (ptrdiff_t) offset, out.size(), out.begin());
        };

        haystack[(size_t) across]     = 0xDE;
        haystack[(size_t) across + 1] = 0xAD;
        haystack[(size_t) across + 2] = 0xBE;
        haystack[(size_t) across + 3] = 0xEF;

        Assert::AreEqual (across, ActionsGoTo::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, 0),
            L"A match that straddles the first chunk's end is found");
        Assert::AreEqual (across, ActionsGoTo::FindInSource (read, haystack.size(), { 0xDE, 0xAD, 0xBE, 0xEF }, false, across + 1),
            L"and a search past it wraps back to it");
        Assert::AreEqual (CassoExplorerActions::kNotFoundOffset, ActionsGoTo::FindInSource (read, haystack.size(), { 0x77 }, false, 0));
    }


    TEST_METHOD (Numbers_ParseSpacesAndCommas)
    {
        std::vector<int>  numbers;

        Assert::IsTrue   (ActionsGoTo::TryParseNumbers (L"17 0 2", 2, numbers));
        Assert::AreEqual ((size_t) 3, numbers.size());
        Assert::AreEqual (17, numbers[0]);
        Assert::IsTrue   (ActionsGoTo::TryParseNumbers (L"17,0", 2, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"17", 2, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"1 2 3 4", 1, numbers));
        Assert::IsFalse  (ActionsGoTo::TryParseNumbers (L"17 x", 1, numbers));
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

    //
    //  A collision inside an image takes Explorer's next free name, in each
    //  file system's legal form, and a copy beside its original says so.
    //
    TEST_METHOD (MakeFreeEntryName_FollowsExplorerInEachFileSystemsForm)
    {
        using A = CassoExplorerActions;

        Assert::AreEqual (std::string ("HELLO"),          A::MakeFreeEntryName ("HELLO", { "OTHER" },                     VolumeKind::Dos33,  false), L"a free name stays");
        Assert::AreEqual (std::string ("HELLO (2)"),      A::MakeFreeEntryName ("HELLO", { "hello" },                     VolumeKind::Dos33,  false));
        Assert::AreEqual (std::string ("HELLO (3)"),      A::MakeFreeEntryName ("HELLO", { "HELLO", "HELLO (2)" },        VolumeKind::Dos33,  false));
        Assert::AreEqual (std::string ("HELLO - COPY"),   A::MakeFreeEntryName ("HELLO", { "HELLO" },                     VolumeKind::Dos33,  true));
        Assert::AreEqual (std::string ("HELLO - COPY (2)"), A::MakeFreeEntryName ("HELLO", { "HELLO", "HELLO - COPY" },   VolumeKind::Dos33,  true));
        Assert::AreEqual (std::string ("HELLO.2"),        A::MakeFreeEntryName ("HELLO", { "HELLO" },                     VolumeKind::ProDos, false));
        Assert::AreEqual (std::string ("HELLO.COPY"),     A::MakeFreeEntryName ("HELLO", { "HELLO" },                     VolumeKind::ProDos, true));
        Assert::AreEqual (std::string ("HELLO.COPY.2"),   A::MakeFreeEntryName ("HELLO", { "HELLO", "HELLO.COPY" },       VolumeKind::ProDos, true));
        Assert::AreEqual (std::string ("ABCDEFGHIJKLM.2"), A::MakeFreeEntryName ("ABCDEFGHIJKLMNO", { "ABCDEFGHIJKLMNO" }, VolumeKind::ProDos, false), L"cut to fit 15");
        Assert::AreEqual (std::string ("ABCDEFGHIJ.COPY"), A::MakeFreeEntryName ("ABCDEFGHIJKLMNO", { "ABCDEFGHIJKLMNO" }, VolumeKind::ProDos, true));
    }
};
