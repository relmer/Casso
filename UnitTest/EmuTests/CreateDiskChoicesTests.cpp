#include "Pch.h"
#include "Ui/Dialogs/CreateDiskDialog.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/MountDiagnosis.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDiskChoicesTests
//
//  What the create dialog offers, against what the builder can actually
//  write.
//
//  THE TWO DRIFTED APART ONCE ALREADY. `.do` was in the command line's table
//  and in the extension reader while the validator refused it, and after that
//  was fixed the dialog still did not list it -- three hand-kept lists of one
//  fact. These tests hold every surface to the builder's own list rather than
//  to a fourth copy written here.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (CreateDiskChoicesTests)
{
public:

    static std::vector<DiskFormat> Writable()
    {
        const DiskFormat *       containers = nullptr;
        size_t                   count      = 0;
        size_t                   i          = 0;
        std::vector<DiskFormat>  all;

        containers = BlankDiskBuilder::GetWritableContainers (count);

        for (i = 0; i < count; i++)
        {
            all.push_back (containers[i]);
        }

        return all;
    }

    static bool Holds (const std::vector<DiskFormat> & list, DiskFormat format)
    {
        return std::find (list.begin(), list.end(), format) != list.end();
    }

    //  Every container the builder can write is offered for SOME filesystem.
    //  A container that no filling can reach is one nobody can create.
    TEST_METHOD (EveryWritableContainerIsOfferedForSomeFilesystem)
    {
        std::vector<DiskFormat>  dos = BlankDiskBuilder::GetContainers (BlankDiskContents::Dos33);
        std::vector<DiskFormat>  pro = BlankDiskBuilder::GetContainers (BlankDiskContents::ProDos);
        std::vector<DiskFormat>  raw = BlankDiskBuilder::GetContainers (BlankDiskContents::Unformatted);

        for (DiskFormat format : Writable())
        {
            Assert::IsTrue (Holds (dos, format) || Holds (pro, format) || Holds (raw, format),
                L"the builder writes a container the dialog never offers");
        }
    }

    //  Raw media carries no sector order, so every container takes it. That
    //  makes the unformatted list the whole set, and a container added to the
    //  builder without a dialog arm fails here.
    TEST_METHOD (UnformattedOffersEveryWritableContainer)
    {
        Assert::AreEqual (Writable().size(),
                          BlankDiskBuilder::GetContainers (BlankDiskContents::Unformatted).size());
    }

    //  .do IS .dsk UNDER THE OTHER NAME, so wherever one is offered the other
    //  must be. This is the case that was missing from the dialog.
    TEST_METHOD (DoIsOfferedWhereverDskIs)
    {
        BlankDiskContents  fillings[] = { BlankDiskContents::Dos33,
                                          BlankDiskContents::ProDos,
                                          BlankDiskContents::Unformatted };

        for (BlankDiskContents contents : fillings)
        {
            std::vector<DiskFormat>  offered = BlankDiskBuilder::GetContainers (contents);

            Assert::AreEqual (Holds (offered, DiskFormat::Dsk), Holds (offered, DiskFormat::Do),
                L"the two spellings of one container must be offered together");
        }
    }

    //  An illegal pairing is never listed, which is what lets ValidateSpec go
    //  on asserting: the dialog cannot express a spec the builder refuses.
    TEST_METHOD (NoOfferedPairingIsOneTheBuilderRefuses)
    {
        BlankDiskContents  fillings[] = { BlankDiskContents::Dos33,
                                          BlankDiskContents::ProDos,
                                          BlankDiskContents::Unformatted };

        for (BlankDiskContents contents : fillings)
        {
            for (DiskFormat format : BlankDiskBuilder::GetContainers (contents))
            {
                BlankDiskSpec  spec;

                spec.contents = contents;
                spec.format   = format;

                Assert::IsTrue (BlankDiskVerdict::Ok == BlankDiskBuilder::CheckSpec (spec),
                    L"the dialog offers a pairing the builder will refuse");
            }
        }
    }

    //  Every container the dialog offers has an extension of its own, checked
    //  on CORE's list because that is now the only list there is. The dialog
    //  used to switch on the format itself, with a default arm answering the
    //  WOZ name, so a container added without an arm was presented as a WOZ
    //  rather than refused -- and living in the executable, which the test
    //  assembly does not link, nothing here could reach it to find out. The
    //  duplication is gone rather than pinned, so this checks the survivor.
    TEST_METHOD (EveryOfferedContainerHasAnExtensionOfItsOwn)
    {
        std::vector<DiskFormat>  offered = Writable();
        size_t                   i       = 0;
        size_t                   j       = 0;

        Assert::IsTrue (offered.size() > 1, L"the sweep must have something to compare");

        for (i = 0; i < offered.size(); i++)
        {
            std::string  ext = MountDiagnosis::GetPrimaryExtension (offered[i]);

            Assert::AreNotEqual ("disk", ext.c_str(),
                L"a writable container fell through to the fallback name");

            for (j = i + 1; j < offered.size(); j++)
            {
                Assert::AreNotEqual (ext.c_str(),
                                     MountDiagnosis::GetPrimaryExtension (offered[j]),
                    L"two writable containers share one extension");
            }
        }
    }

    //  The names an interface shows, now that they are reachable. They lived
    //  in the create dialog until this feature, where the test assembly could
    //  not link them -- so the arm-less default returning the WOZ name went
    //  unnoticed. Moving them into core is what makes this test possible.
    TEST_METHOD (EveryWritableContainerHasItsOwnCaptionAndWideExtension)
    {
        std::vector<DiskFormat>  offered = Writable();
        size_t                   i       = 0;
        size_t                   j       = 0;

        Assert::IsTrue (offered.size() > 1, L"the sweep must have something to compare");

        for (i = 0; i < offered.size(); i++)
        {
            std::wstring  ext     = MountDiagnosis::GetPrimaryExtensionText (offered[i]);
            std::wstring  caption = MountDiagnosis::GetContainerCaption (offered[i]);

            Assert::AreNotEqual (std::wstring (L".disk").c_str(), ext.c_str(),
                L"a writable container fell through to the fallback name");
            Assert::IsTrue (ext.size() > 1 && ext[0] == L'.',
                L"an extension is a dot and at least one character");

            //  The caption IS the extension, so they cannot name a container
            //  two different things.
            Assert::AreEqual (ext.substr (1).size(), caption.size(),
                L"the caption must be the extension without its dot");

            for (j = 0; j < caption.size(); j++)
            {
                Assert::AreEqual ((int) towupper (ext[j + 1]), (int) caption[j],
                    L"the caption must be the extension, capitalized");
            }

            for (j = i + 1; j < offered.size(); j++)
            {
                Assert::AreNotEqual (caption.c_str(),
                                     MountDiagnosis::GetContainerCaption (offered[j]).c_str(),
                    L"two writable containers share one caption");
            }
        }
    }

    //  Every filling has a caption of its own. The create dialog held this as
    //  a switch with a default arm answering "DOS 3.3", so a filling added
    //  without an arm was mislabeled rather than caught -- and no test could
    //  reach it. The core version has no default arm at all, so the next
    //  enumerator fails to compile; this checks the ones that exist.
    TEST_METHOD (EveryFillingHasACaptionOfItsOwn)
    {
        BlankDiskContents  fillings[] = { BlankDiskContents::Unformatted,
                                          BlankDiskContents::Dos33,
                                          BlankDiskContents::ProDos };
        size_t             count      = _countof (fillings);
        size_t             i          = 0;
        size_t             j          = 0;

        Assert::IsTrue (count > 1, L"the sweep must have something to compare");

        for (i = 0; i < count; i++)
        {
            std::wstring  caption = BlankDiskBuilder::GetContentsCaption (fillings[i]);

            Assert::IsFalse (caption.empty(), L"a filling with no caption of its own");

            for (j = i + 1; j < count; j++)
            {
                Assert::AreNotEqual (caption.c_str(),
                                     BlankDiskBuilder::GetContentsCaption (fillings[j]).c_str(),
                    L"two fillings share one caption, so one is being mislabeled");
            }
        }
    }

    //  The command line and the dialog write the same containers. Neither is
    //  the authority; the builder is, and both are checked against it.
    TEST_METHOD (TheCommandLineAdvertisesExactlyWhatTheBuilderWrites)
    {
        const DiskCommandRunner::ContainerName *  words = nullptr;
        size_t                                    count = 0;
        size_t                                    i     = 0;
        std::vector<DiskFormat>                   advertised;

        words = DiskCommandRunner::GetAdvertisedContainers (count);

        for (i = 0; i < count; i++)
        {
            advertised.push_back (words[i].format);
        }

        //  COVERAGE BOTH WAYS, NOT EQUAL COUNTS. This compared sizes until a
        //  container arrived with two spellings: nibble images answer to both
        //  `nib` and `nb2`, which differ only in track size and share one
        //  DiskFormat, so six words map to five formats. Counting made that
        //  read as a disagreement when the two lists agree completely. What
        //  actually matters is that neither side holds something the other
        //  does not.
        for (DiskFormat format : advertised)
        {
            Assert::IsTrue (Holds (Writable(), format),
                L"the command line offers a word for a container the builder cannot write");
        }

        for (DiskFormat format : Writable())
        {
            Assert::IsTrue (Holds (advertised, format),
                L"the builder writes a container the command line has no word for");
        }
    }
};
