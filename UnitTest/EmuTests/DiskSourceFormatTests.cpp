#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSourceFormatTests
//
//  Extension routing and load-failure classification, from file paths and
//  bytes alone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskSourceFormatTests)
{
public:

    // A minimal single-track WOZ v2 image with a recognizable bit pattern.
    static vector<Byte> MakeWoz()
    {
        vector<Byte>   bits (6400, 0xFF);
        vector<Byte>   woz;

        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        WozLoader::BuildSyntheticV2 (1, false, bits, 51200, woz);
        return woz;
    }

    TEST_METHOD (GetSourceFormatByExtension_KnownTypes)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        AssertSucceeded (DiskSourceFormat::GetSourceFormatByExtension ("foo.dsk", fmt));
        Assert::IsTrue (fmt == DiskFormat::Dsk);

        AssertSucceeded (DiskSourceFormat::GetSourceFormatByExtension ("foo.DO", fmt));
        Assert::IsTrue (fmt == DiskFormat::Do);

        AssertSucceeded (DiskSourceFormat::GetSourceFormatByExtension ("foo.po", fmt));
        Assert::IsTrue (fmt == DiskFormat::Po);

        AssertSucceeded (DiskSourceFormat::GetSourceFormatByExtension ("foo.WOZ", fmt));
        Assert::IsTrue (fmt == DiskFormat::Woz);
    }

    TEST_METHOD (GetSourceFormatByExtension_UnknownReturnsFail)
    {
        DiskFormat   fmt = DiskFormat::Dsk;

        AssertFailed (DiskSourceFormat::GetSourceFormatByExtension ("foo.bin", fmt));
        AssertFailed (DiskSourceFormat::GetSourceFormatByExtension ("noext",  fmt));
    }

    TEST_METHOD (IsMountableImageExtension_MatchesTheRoutedTypes)
    {
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (string ("foo.dsk")));
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (string ("foo.DO")));
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (string ("foo.po")));
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (string ("foo.WOZ")));

        Assert::IsFalse (DiskSourceFormat::IsMountableImageExtension (string ("foo.bin")));
        Assert::IsFalse (DiskSourceFormat::IsMountableImageExtension (string ("noext")));
    }

    TEST_METHOD (EveryDiskFormat_HasAnExtensionThatRoutesBackToIt)
    {
        //  THE SWEEP IS OVER THE ENUM, NOT OVER A LIST OF FORMATS. A list
        //  visits only the rows its author remembered, so it cannot find the
        //  arm they forgot -- which is the failure this guards. There is a
        //  live example in the tree: BlankDiskBuilder::ValidateSpec has no
        //  DiskFormat::Do arm and asserts on a container the tool advertises,
        //  and no table-driven test noticed because no table lists it.
        //
        //  Both directions matter. A format with no extension arm answers the
        //  "disk" fallback; one with no routing arm fails to come back.
        int          count  = (int) DiskFormat::Count;
        int          i      = 0;
        DiskFormat   fmt    = DiskFormat::Dsk;
        DiskFormat   routed = DiskFormat::Dsk;
        const char * ext    = nullptr;
        std::string  path;
        HRESULT      hr     = S_OK;

        Assert::IsTrue (count > 0, L"the enum must not be empty");

        for (i = 0; i < count; i++)
        {
            fmt = (DiskFormat) i;
            ext = MountDiagnosis::GetPrimaryExtension (fmt);

            Assert::AreNotEqual ("disk", ext,
                L"every format needs its own extension, not the fallback");

            path = std::string ("image") + ext;
            hr   = DiskSourceFormat::GetSourceFormatByExtension (path, routed);

            AssertSucceeded (hr, L"a format's own primary extension must route");
            Assert::AreEqual ((int) fmt, (int) routed,
                L"and must route back to the format it came from");
        }
    }

    TEST_METHOD (IsMountableImageExtension_AcceptsNibbleImages)
    {
        // INVERTED, NOT DELETED. Mount now routes both nibble extensions, and
        // the assertion that it did not belongs turned around rather than
        // removed -- the filter and the router disagreeing over these names is
        // the exact defect this test was written for.
        Assert::IsTrue (DiskSourceFormat::IsMountableImageExtension (string ("foo.nib")));
        Assert::IsTrue (DiskSourceFormat::IsMountableImageExtension (string ("foo.nb2")));
        Assert::IsTrue (DiskSourceFormat::IsMountableImageExtension (wstring (L"foo.NIB")));
        Assert::IsTrue (DiskSourceFormat::IsMountableImageExtension (wstring (L"foo.Nb2")));
    }

    TEST_METHOD (IsMountableImageExtension_WideAgreesWithNarrow)
    {
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (wstring (L"C:\\Disks\\BOOT.DSK")));
        Assert::IsTrue  (DiskSourceFormat::IsMountableImageExtension (wstring (L"C:\\Disks\\demo.woz")));
        Assert::IsFalse (DiskSourceFormat::IsMountableImageExtension (wstring (L"C:\\Disks\\notes.txt")));
        Assert::IsFalse (DiskSourceFormat::IsMountableImageExtension (wstring (L"")));
    }

    TEST_METHOD (ClassifyLoadFailure_ShortSectorImage_NamesTheLength)
    {
        vector<Byte>    truncated (4096, 0);
        MountDiagnosis  diagnosis = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Dsk,
                                                                         truncated);



        Assert::IsTrue (diagnosis.failure == MountFailure::WrongSizeForFormat,
            L"a sector image of the wrong length is diagnosed by its length");
        Assert::AreEqual ((size_t) 4096, diagnosis.fileByteSize,
            L"the size the user's file actually was must survive to the message");
    }

    TEST_METHOD (ClassifyLoadFailure_NibbleOfTheWrongLength_NamesItsOwnSizes)
    {
        vector<Byte>    truncated (100000, 0xFF);
        MountDiagnosis  diagnosis = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Nib,
                                                                         truncated);
        std::string     clause    = diagnosis.Describe();

        Assert::IsTrue (diagnosis.failure == MountFailure::WrongSizeForNibble,
            L"a nibble image has its own size rule and its own verdict");
        Assert::AreEqual ((size_t) 100000, diagnosis.fileByteSize);

        //  BOTH accepted totals, because either can carry either name. A
        //  clause naming one of them tells half the users the wrong thing.
        Assert::IsTrue (clause.find ("232,960") != std::string::npos,
            L"the clause must give the standard total");
        Assert::IsTrue (clause.find ("223,440") != std::string::npos,
            L"and the smaller one, which circulates under the same name");
        Assert::IsTrue (clause.find ("100,000") != std::string::npos,
            L"and the size the user's file actually was");
    }

    TEST_METHOD (ClassifyLoadFailure_RightSizedButNoNibbles_IsItsOwnVerdict)
    {
        vector<Byte>    zeros (NibbleImageCodec::kNibImageSize, 0x00);
        MountDiagnosis  wrongSize = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Nib,
                                                                         vector<Byte> (999, 0xFF));
        MountDiagnosis  noNibbles = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Nib, zeros);

        //  The two nibble refusals must not read alike: one says the download
        //  stopped early, the other says the file was renamed from something
        //  else, and a user can act on each.
        Assert::IsTrue (noNibbles.failure == MountFailure::NotANibbleStream,
            L"right size, no assemblable nibble anywhere");
        Assert::AreNotEqual (wrongSize.Describe(), noNibbles.Describe(),
            L"the two nibble refusals must differ");
    }

    TEST_METHOD (ClassifyLoadFailure_NibbleGarbageOfTheRightLength_IsAccepted)
    {
        //  DELIBERATELY NOT REFUSED, and asserted so nobody later "fixes" it.
        //  The format has no signature, header or checksum, and about half of
        //  random bytes carry the high bit, so a renamed archive of the right
        //  length is indistinguishable from a real image by inspection. It
        //  mounts as a disk that will not boot, which is the honest outcome; a
        //  stricter rule would refuse odd but genuine images to catch files
        //  that fail harmlessly anyway.
        vector<Byte>    garbage (NibbleImageCodec::kNibImageSize, 0);
        size_t          i = 0;

        for (i = 0; i < garbage.size(); i++)
        {
            garbage[i] = static_cast<Byte> ((i * 37) & 0xFF);
        }

        MountDiagnosis  diagnosis = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Nib, garbage);

        Assert::IsTrue (diagnosis.failure == MountFailure::Unrecognized,
            L"content this loader cannot rule out is not given a nibble verdict");
    }

    TEST_METHOD (ClassifyLoadFailure_EmptyFile_OutranksTheLengthTest)
    {
        vector<Byte>    nothing;
        MountDiagnosis  asDsk = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Dsk, nothing);
        MountDiagnosis  asWoz = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Woz, nothing);



        // A zero-byte file is the wrong size for every container, and saying
        // so by arithmetic against 143,360 buries the one useful fact.
        Assert::IsTrue (asDsk.failure == MountFailure::EmptyFile,
            L"an empty sector image is empty before it is the wrong size");
        Assert::IsTrue (asWoz.failure == MountFailure::EmptyFile,
            L"and an empty .woz is empty before it is a missing header");
    }

    TEST_METHOD (ClassifyLoadFailure_WozWithoutAHeader_IsNotAWozAtAll)
    {
        vector<Byte>    renamed (600, 0x41);
        MountDiagnosis  diagnosis = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Woz, renamed);



        Assert::IsTrue (diagnosis.failure == MountFailure::NotAWozFile,
            L"bytes with no WOZ header are not a damaged WOZ, they are not one");
    }

    TEST_METHOD (ClassifyLoadFailure_WozWithAHeader_IsADamagedWoz)
    {
        vector<Byte>    woz     = MakeWoz();
        size_t          keep    = WozLoader::kHeaderSize + 16;
        vector<Byte>    chopped (woz.begin(), woz.begin() + (ptrdiff_t) keep);
        MountDiagnosis  diagnosis;



        // The header survives the truncation, so what is left IS a WOZ -- an
        // incomplete one. Telling the user it is not a WOZ would send them
        // looking for a file they already have.
        diagnosis = DiskSourceFormat::ClassifyLoadFailure (DiskFormat::Woz, chopped);

        Assert::IsTrue (diagnosis.failure == MountFailure::MalformedWoz,
            L"a real WOZ header over broken chunks is a damaged image");
    }
};
