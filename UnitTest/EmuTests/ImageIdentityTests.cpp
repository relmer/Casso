#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/ImageIdentity.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageIdentityTests
//
//  What "has this file changed since I read it" answers, and what it refuses to
//  answer.
//
//  THE RECORDED FLAG IS THE SUBJECT OF HALF OF THESE. A zero size and a zero
//  time are both legal values, so an identity that was never read must not
//  compare equal to one that was -- otherwise a stat that failed reads as
//  proof that nothing changed, which is the one wrong answer this whole
//  mechanism exists to avoid.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageIdentityTests)
{
public:

    static ImageIdentity  Recorded (uint64_t size, int64_t modified)
    {
        ImageIdentity  identity;



        identity.stamp.sizeBytes    = size;
        identity.stamp.modifiedUnix = modified;
        identity.recorded           = true;

        return identity;
    }



    TEST_METHOD (TwoRecordedIdentitiesWithTheSameStampMatch)
    {
        ImageIdentity  left  = Recorded (140 * 1024, 5000);
        ImageIdentity  right = Recorded (140 * 1024, 5000);



        Assert::IsTrue (left.Matches (right), L"same size and time is the same file");
        Assert::IsTrue (right.Matches (left), L"the comparison is symmetric");
    }



    TEST_METHOD (ADifferentSizeAloneIsAChange)
    {
        ImageIdentity  atMount = Recorded (140 * 1024, 5000);
        ImageIdentity  now     = Recorded (143 * 1024, 5000);



        Assert::IsFalse (atMount.Matches (now),
                         L"a file that grew has changed even at the same timestamp");
    }



    TEST_METHOD (ADifferentTimeAloneIsAChange)
    {
        ImageIdentity  atMount = Recorded (140 * 1024, 5000);
        ImageIdentity  now     = Recorded (140 * 1024, 5001);



        //  The case the whole feature is built for: a disk image rewritten in
        //  place keeps its size exactly, because the format's length is fixed.
        Assert::IsFalse (atMount.Matches (now),
                         L"a same-size rewrite is still a change");
    }



    TEST_METHOD (AnUnrecordedIdentityMatchesNothing)
    {
        ImageIdentity  never;
        ImageIdentity  real = Recorded (140 * 1024, 5000);



        Assert::IsFalse (never.Matches (real),  L"never having looked is not evidence");
        Assert::IsFalse (real.Matches (never),  L"and it is not evidence from either side");
    }



    TEST_METHOD (TwoUnrecordedIdentitiesDoNotMatchEachOther)
    {
        ImageIdentity  left;
        ImageIdentity  right;



        //  Both are default-constructed, so every field they carry is equal. A
        //  comparison that ignored the flag would call this a match and report
        //  "nothing changed" about two stats that never ran.
        Assert::IsFalse (left.Matches (right),
                         L"two absences of evidence are not evidence");
    }



    TEST_METHOD (ADefaultIdentityIsNotRecorded)
    {
        ImageIdentity  identity;



        Assert::IsFalse (identity.recorded, L"nothing has been read yet");
    }



    TEST_METHOD (ReadThroughTheSeamRecordsWhatStatReports)
    {
        FakeDiskFileIo  io;
        ImageIdentity   identity;



        io.files["A.dsk"]  = std::vector<Byte> (10, 0);
        io.stamps["A.dsk"] = FileStamp { 10, 77 };

        identity = ImageIdentity::Read (io, "A.dsk");

        Assert::IsTrue (identity.recorded, L"the stat succeeded");
        Assert::AreEqual ((uint64_t) 10, identity.stamp.sizeBytes);
        Assert::AreEqual ((int64_t)  77, identity.stamp.modifiedUnix);
    }



    TEST_METHOD (ReadOfAFileThatIsNotThereIsUnrecorded)
    {
        FakeDiskFileIo  io;
        ImageIdentity   identity  = ImageIdentity::Read (io, "Gone.dsk");
        ImageIdentity   atMount   = Recorded (140 * 1024, 5000);



        Assert::IsFalse (identity.recorded,
                         L"a file that is gone stats as absent, not as zero bytes");

        //  The distinction that matters: gone must not read as unchanged.
        Assert::IsFalse (atMount.Matches (identity),
                         L"a deleted image has not stayed the same");
    }



    TEST_METHOD (ReadFromTheFileSystemOfAMissingPathIsUnrecorded)
    {
        ImageIdentity  identity =
            ImageIdentity::ReadFromFileSystem ("Z:\\no\\such\\place\\Nothing.dsk");



        Assert::IsFalse (identity.recorded, L"nothing to stat");
    }



    TEST_METHOD (ReadFromTheFileSystemOfAnEmptyPathIsUnrecorded)
    {
        ImageIdentity  identity = ImageIdentity::ReadFromFileSystem ("");



        Assert::IsFalse (identity.recorded, L"an unnamed file has no identity");
    }
};
