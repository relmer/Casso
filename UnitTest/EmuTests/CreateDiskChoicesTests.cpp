#include "Pch.h"
#include "Ui/Dialogs/CreateDiskDialog.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskCommandRunner.h"

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

        containers = BlankDiskBuilder::WritableContainers (count);

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
        std::vector<DiskFormat>  dos = BlankDiskBuilder::ContainersFor (BlankDiskContents::Dos33);
        std::vector<DiskFormat>  pro = BlankDiskBuilder::ContainersFor (BlankDiskContents::ProDos);
        std::vector<DiskFormat>  raw = BlankDiskBuilder::ContainersFor (BlankDiskContents::Unformatted);

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
                          BlankDiskBuilder::ContainersFor (BlankDiskContents::Unformatted).size());
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
            std::vector<DiskFormat>  offered = BlankDiskBuilder::ContainersFor (contents);

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
            for (DiskFormat format : BlankDiskBuilder::ContainersFor (contents))
            {
                BlankDiskSpec  spec;

                spec.contents = contents;
                spec.format   = format;

                Assert::IsTrue (BlankDiskVerdict::Ok == BlankDiskBuilder::CheckSpec (spec),
                    L"the dialog offers a pairing the builder will refuse");
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

        words = DiskCommandRunner::AdvertisedContainers (count);

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
