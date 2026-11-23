#include "Pch.h"

#include "Devices/Disk/BlankDiskSpecResolver.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/Store/DiskStoreMessages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMessagesTests
//
//  The wording of each mount and save notice.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskStoreMessagesTests)
{
public:

    //  Builds a diagnosis with a single reason for the message tests.
    static MountDiagnosis  DiagnosisOf (MountFailure failure,
                                        DiskFormat   format = DiskFormat::Dsk,
                                        size_t       fileByteSize = 0)
    {
        MountDiagnosis  diagnosis;



        diagnosis.failure      = failure;
        diagnosis.format       = format;
        diagnosis.fileByteSize = fileByteSize;

        return diagnosis;
    }

    TEST_METHOD (FormatMountFailureMessage_UnknownExtension_ListsWhatIsRead)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Notes.txt",
                               DiagnosisOf (MountFailure::UnknownExtension));



        Assert::IsTrue (message.find (L"C:\\disks\\Notes.txt") != wstring::npos,
            L"the message must identify the file the user picked");

        //  Every container the tool advertises, read off the same table the
        //  tool reads -- a name added there and left out here is exactly the
        //  drift this used to have, when the sentence named four of five.
        {
            size_t  count = 0;
            const BlankDiskSpecResolver::ContainerName *  names =
                BlankDiskSpecResolver::GetAdvertisedContainers (count);

            Assert::IsTrue (count >= 5, L"the table is not shorter than what shipped");

            for (size_t i = 0; i < count; i++)
            {
                wstring  dotted = L"." + wstring (names[i].name, names[i].name + strlen (names[i].name));

                Assert::IsTrue (message.find (dotted) != wstring::npos,
                    (L"the refusal must name " + dotted).c_str());
            }
        }
    }

    TEST_METHOD (FormatMountFailureMessage_KnownExtension_BlamesTheContents)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Broken.dsk",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Dsk, 4096));



        Assert::IsTrue (message.find (L"C:\\disks\\Broken.dsk") != wstring::npos,
            L"the message must identify the file the user picked");

        // A .dsk IS read, so reciting the supported formats back would be
        // both useless and misleading -- the extension was never the problem.
        Assert::IsTrue (message.find (L".woz") == wstring::npos,
            L"a recognized extension must not be met with the format list");
    }

    TEST_METHOD (FormatMountFailureMessage_WrongSize_GivesBothNumbers)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Truncated.dsk",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Dsk, 4096));



        // "Not a valid disk image" is barely better than saying nothing. The
        // two numbers together are what tell somebody their download stopped
        // early, which is the whole reason the size is carried this far.
        Assert::IsTrue (message.find (L"4,096 bytes") != wstring::npos,
            L"the message must report how big the file actually is");
        Assert::IsTrue (message.find (L"143,360 bytes") != wstring::npos,
            L"and how big it should have been");
        Assert::IsTrue (message.find (L".dsk") != wstring::npos,
            L"identified as the container the file's own name promised");
    }

    TEST_METHOD (FormatMountFailureMessage_35InchImage_IsAboutTheDrive)
    {
        wstring  sized = DiskStoreMessages::FormatMountFailureMessage (
                             "C:\\disks\\Big.dsk",
                             DiagnosisOf (MountFailure::WrongSizeForFormat,
                                          DiskFormat::Dsk, 800 * 1024));
        wstring  woz   = DiskStoreMessages::FormatMountFailureMessage (
                             "C:\\disks\\Big.woz",
                             DiagnosisOf (MountFailure::WozNot525,
                                          DiskFormat::Woz, 1000));



        Assert::IsTrue (sized.ends_with (L"\n\nCannot mount a 3.5-inch image in a 5.25-inch drive."), sized.c_str());
        Assert::IsTrue (woz.ends_with   (L"\n\nCannot mount a 3.5-inch image in a 5.25-inch drive."), woz.c_str());
    }

    TEST_METHOD (FormatMountFailureMessage_WrongSize_NamesTheFormatItWasGiven)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Half.po",
                               DiagnosisOf (MountFailure::WrongSizeForFormat,
                                            DiskFormat::Po, 71680));



        Assert::IsTrue (message.find (L".po") != wstring::npos,
            L"a .po must not be told what a .dsk should weigh");
        Assert::IsTrue (message.find (L"71,680 bytes") != wstring::npos);
    }

    TEST_METHOD (FormatMountFailureMessage_Unreadable_PointsAtTheFileNotTheContents)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Gone.dsk",
                               DiagnosisOf (MountFailure::FileUnreadable, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"cannot be read") != wstring::npos,
            L"a file that never opened must be reported as a file, not as a bad image");
        Assert::IsTrue (message.find (L"143,360") == wstring::npos,
            L"and must not be lectured about a size nobody measured");
    }

    TEST_METHOD (FormatMountFailureMessage_EmptyFile_SaysSoPlainly)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Zero.dsk",
                               DiagnosisOf (MountFailure::EmptyFile, DiskFormat::Dsk));



        Assert::IsTrue (message.find (L"empty") != wstring::npos,
            L"a zero-byte file is empty, which is more use than an arithmetic complaint");
    }

    TEST_METHOD (FormatMountFailureMessage_NotAWoz_SuggestsARename)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));



        Assert::IsTrue (message.find (L"WOZ file header") != wstring::npos,
            L"the missing header is the fact, and reporting it is what explains the refusal");
        Assert::IsTrue (message.find (L"renamed") != wstring::npos,
            L"a .woz that is not a WOZ is almost always a renamed file, so report that");
    }

    TEST_METHOD (FormatMountFailureMessage_MalformedWoz_SaysTheFileIsDamaged)
    {
        wstring  notAWoz = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Renamed.woz",
                               DiagnosisOf (MountFailure::NotAWozFile, DiskFormat::Woz));
        wstring  damaged = DiskStoreMessages::FormatMountFailureMessage (
                               "C:\\disks\\Damaged.woz",
                               DiagnosisOf (MountFailure::MalformedWoz, DiskFormat::Woz));



        Assert::IsTrue (damaged.find (L"damaged or incomplete") != wstring::npos,
            L"a real WOZ header over broken chunks is a damaged file, and reads as one");

        // The two send the user in different directions: one to find the real
        // image, the other to fetch this one again. Sharing wording would
        // throw that away.
        Assert::AreNotEqual (notAWoz, damaged,
            L"a renamed file and a damaged one must not read alike");
    }

    TEST_METHOD (FormatMountFailureMessage_EmptyPath_StillReadsAsASentence)
    {
        wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                               string(), DiagnosisOf (MountFailure::Unrecognized));



        Assert::IsTrue (message.find (L"(unknown path)") != wstring::npos,
            L"a missing path must be marked as missing, not left as a blank gap");
    }

    TEST_METHOD (FormatMountFailureMessage_EveryReason_SaysSomethingOfItsOwn)
    {
        //  MountFailure is TOTAL over the wordings, so the sweep runs the
        //  enum rather than a table of the ones somebody remembered. A table
        //  sweep visits only rows that exist and structurally cannot find a
        //  missing one.
        const MountFailure  reasons[] =
        {
            MountFailure::None,
            MountFailure::UnknownExtension,
            MountFailure::FileUnreadable,
            MountFailure::EmptyFile,
            MountFailure::WrongSizeForFormat,
            MountFailure::NotAWozFile,
            MountFailure::MalformedWoz,
            MountFailure::Unrecognized,
        };

        vector<wstring>  seen;
        size_t           i    = 0;
        size_t           j    = 0;



        Assert::AreEqual ((size_t) 8, sizeof (reasons) / sizeof (reasons[0]),
            L"an enumerator added without a wording must fail here, not ship silent");

        for (i = 0; i < sizeof (reasons) / sizeof (reasons[0]); i++)
        {
            wstring  message = DiskStoreMessages::FormatMountFailureMessage (
                                   "C:\\disks\\Image.dsk",
                                   DiagnosisOf (reasons[i], DiskFormat::Dsk, 4096));

            Assert::IsTrue (message.find (L"C:\\disks\\Image.dsk") != wstring::npos,
                L"every reason names the file");

            for (j = 0; j < seen.size(); j++)
            {
                Assert::AreNotEqual (seen[j], message,
                    L"two reasons sharing one sentence is the failure being fixed");
            }

            seen.push_back (message);
        }
    }

    TEST_METHOD (FormatDamagedImageMessage_DamagedTracks_ReportsTheBlankTracks)
    {
        wstring  tracks   = DiskStoreMessages::FormatDamagedImageMessage ("C:\\disks\\Bad.woz", true);
        wstring  checksum = DiskStoreMessages::FormatDamagedImageMessage ("C:\\disks\\Bad.woz", false);



        Assert::IsTrue (tracks.find (L"C:\\disks\\Bad.woz") != wstring::npos);
        Assert::IsTrue (tracks.find (L"blank ones") != wstring::npos, tracks.c_str());
        Assert::IsTrue (checksum.find (L"newly computed checksum") != wstring::npos, checksum.c_str());
        Assert::AreNotEqual (tracks, checksum);
    }


    TEST_METHOD (FormatFlushLossMessage_WithRecoveryCopy_GivesItsPath)
    {
        wstring  message = DiskStoreMessages::FormatFlushLossMessage ("C:\\disks\\Work.dsk",
                                                                      E_ACCESSDENIED,
                                                                      "C:\\disks\\Work.recovered.woz");



        Assert::IsTrue (message.find (L"C:\\disks\\Work.dsk") != wstring::npos);
        Assert::IsTrue (message.find (L"C:\\disks\\Work.recovered.woz") != wstring::npos, message.c_str());
        Assert::IsTrue (message.find (L"Error: ") != wstring::npos, L"the cause is printed");
        Assert::IsTrue (message.find (L"have not been saved") == wstring::npos);
    }


    TEST_METHOD (FormatFlushLossMessage_WithoutRecoveryCopy_SaysTheDriveHasTheWrites)
    {
        wstring  message = DiskStoreMessages::FormatFlushLossMessage ("C:\\disks\\Work.dsk", E_ACCESSDENIED);



        Assert::IsTrue (message.find (L"The disk in the drive still has them.") != wstring::npos, message.c_str());
        Assert::IsTrue (message.find (L"preserved here") == wstring::npos);
    }


    TEST_METHOD (FormatDiscardedWritesMessage_GivesBothPathsAndSaysTheChangesAreGone)
    {
        wstring  message = DiskStoreMessages::FormatDiscardedWritesMessage ("C:\\disks\\Work.dsk",
                                                                            "C:\\disks\\Work copy.dsk",
                                                                            E_ACCESSDENIED);



        Assert::IsTrue (message.find (L"C:\\disks\\Work.dsk") != wstring::npos);
        Assert::IsTrue (message.find (L"C:\\disks\\Work copy.dsk") != wstring::npos);
        Assert::IsTrue (message.find (L"cannot be recovered") != wstring::npos, message.c_str());
    }


    TEST_METHOD (FormatSalvageFailedMessage_SaysTheOriginalIsUntouched)
    {
        wstring  message = DiskStoreMessages::FormatSalvageFailedMessage ("C:\\disks\\Bad.salvaged.woz");



        Assert::IsTrue (message.find (L"C:\\disks\\Bad.salvaged.woz") != wstring::npos);
        Assert::IsTrue (message.find (L"original disk image was not changed") != wstring::npos);
    }


    TEST_METHOD (FormatExternalChangeMessage_SaysTheWritesAreStillInMemory)
    {
        wstring  message = DiskStoreMessages::FormatExternalChangeMessage (string());



        Assert::IsTrue (message.find (L"(unknown path)") != wstring::npos);
        Assert::IsTrue (message.find (L"still held in memory") != wstring::npos, message.c_str());
    }
};
