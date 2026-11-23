#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartupProgramTests
//
//  ProDosVolume::SetStartupProgram: the system program moved to the front of
//  the volume directory, and the files that cannot be moved there.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosStartupProgramTests)
{
public:

    //
    //  ------------------------------------------------------------------
    //  The startup program, which on this filesystem is a POSITION rather than
    //  a stored name: the kernel launches the first system program its walk of
    //  the volume directory reaches.
    //  ------------------------------------------------------------------
    //

    //  A system program. What the bytes are does not matter here -- which
    //  record the walk reaches first is the whole subject -- but the type does.
    static FilePayload MakeSystemPayload (size_t length)
    {
        FilePayload  payload = ProDosVolumeFixtures::MakeBinaryPayload (length, 0x2000);

        payload.type = ProDosVolume::kTypeSystem;

        return payload;
    }

    //  A blank volume with PRODOS as its first record and BASIC.SYSTEM after it.
    static vector<Byte> MakeVolumeWithKernel()
    {
        vector<Byte>  disk = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  next;

        for (const char * name : { "PRODOS", "BASIC.SYSTEM" })
        {
            ProDosVolume  volume (disk);

            AssertSucceeded (volume.Write (FilePath::Parse (name), MakeSystemPayload (60), next));

            disk = next;
        }

        return disk;
    }

    static size_t IndexOfEntry (const VolumeListing & listing, const std::string & name)
    {
        size_t  i = 0;

        for (i = 0; i < listing.entries.size(); i++)
        {
            if (listing.entries[i].name == name)
            {
                return i;
            }
        }

        Assert::Fail (L"the listing must carry the entry being located");

        return 0;
    }

    //  Every buffer byte one directory record occupies. A record is NOT
    //  contiguous in the image -- a ProDOS block is two 256-byte halves that sit
    //  apart in a DOS-ordered buffer -- so the range has to be mapped rather
    //  than assumed.
    static void AddRecordBytes (std::vector<char>  & inOutAllowed,
                                int                  dirBlock,
                                int                  slot)
    {
        size_t  i = 0;

        for (i = 0; i < ProDosVolumeFixtures::kEntryLength; i++)
        {
            inOutAllowed[ProDosSkeleton::GetBlockByteOffset (dirBlock, ProDosVolumeFixtures::EntryOffset (slot) + i)] = 1;
        }
    }

    vector<Byte> MakeVolumeWithTwoSystemPrograms()
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  one;
        vector<Byte>  two;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("FIRST.SYSTEM"),
                                           MakeSystemPayload (600), one));
        }

        {
            ProDosVolume  volume (one);

            AssertSucceeded (volume.Write (FilePath::Parse ("SECOND.SYSTEM"),
                                           MakeSystemPayload (600), two));
        }

        return two;
    }

    TEST_METHOD (Volume_SetStartupProgram_PutsTheChosenProgramInFrontOfEveryOtherSystemFile)
    {
        // The reorder is the mechanism, so the assertion is about ORDER -- and
        // about what did not move. Every other record keeps its bytes, which is
        // checked by requiring every differing byte in the whole image to fall
        // inside one of the two records that were swapped.
        vector<Byte>       before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume       volume (before);
        vector<Byte>       after;
        VolumeListing      listedBefore;
        VolumeListing      listedAfter;
        std::vector<char>  allowed (NibbleFormat::kImageByteSize, 0);
        size_t             differing = 0;
        size_t             i         = 0;

        AssertSucceeded (volume.Enumerate (listedBefore));

        Assert::AreEqual (size_t (0), IndexOfEntry (listedBefore, "FIRST.SYSTEM"),
            L"the volume must start out launching the other one, or nothing is being moved");

        AssertSucceeded (volume.SetStartupProgram (FilePath::Parse ("SECOND.SYSTEM"), after));

        {
            ProDosVolume  written (after);

            AssertSucceeded (written.Enumerate (listedAfter));
        }

        Assert::AreEqual (size_t (0), IndexOfEntry (listedAfter, "SECOND.SYSTEM"),
            L"the chosen program must now be the first record the walk reaches");

        Assert::AreEqual (listedBefore.entries.size(), listedAfter.entries.size(),
            L"and no entry may be gained or lost by reordering two of them");

        Assert::AreEqual (listedBefore.freeUnits, listedAfter.freeUnits,
            L"a swap allocates and frees nothing");

        AddRecordBytes (allowed, ProDosVolumeFixtures::kDirKeyBlock, 1);
        AddRecordBytes (allowed, ProDosVolumeFixtures::kDirKeyBlock, 2);

        for (i = 0; i < before.size(); i++)
        {
            if (before[i] != after[i])
            {
                differing++;

                Assert::AreEqual ((char) 1, allowed[i],
                    L"nothing outside the two swapped records may change -- not a block "
                    L"pointer, not the bitmap, not another file's entry");
            }
        }

        Assert::IsTrue (differing > 0, L"and the records must actually have been swapped");
    }

    TEST_METHOD (Volume_SetStartupProgram_AProgramAlreadyInFront_LeavesTheVolumeByteForByteAsItWas)
    {
        // Nothing to do is a legitimate outcome here, and rewriting the same
        // arrangement back would make an unnecessary commit look like an edit.
        vector<Byte>  before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume  volume (before);
        vector<Byte>  after;

        AssertSucceeded (volume.SetStartupProgram (FilePath::Parse ("FIRST.SYSTEM"), after));

        Assert::IsTrue (after == before,
            L"a program that is already first leaves the volume exactly as it was");
    }

    TEST_METHOD (Volume_SetStartupProgram_AFileThatIsNotASystemProgram_IsRefused)
    {
        // A BIN file cannot be launched by the boot path at any position, so
        // reordering the directory would produce a disk that looks configured
        // and boots to whatever the next system program happens to be.
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  written;
        vector<Byte>  after;
        HRESULT       hr  = S_OK;

        {
            ProDosVolume  volume (vol);

            AssertSucceeded (volume.Write (FilePath::Parse ("PROG"),
                                           ProDosVolumeFixtures::MakeBinaryPayload (600, 0x6000), written));
        }

        {
            ProDosVolume  volume (written);

            hr = volume.SetStartupProgram (FilePath::Parse ("PROG"), after);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE), hr);
        Assert::AreEqual (size_t (0), after.size(), L"and a refusal produces nothing");
    }

    TEST_METHOD (Volume_SetStartupProgram_AFileTheVolumeDoesNotHold_IsRefusedAndProducesNothing)
    {
        vector<Byte>  before = MakeVolumeWithTwoSystemPrograms();
        ProDosVolume  volume (before);
        vector<Byte>  after;
        HRESULT       hr     = S_OK;

        hr = volume.SetStartupProgram (FilePath::Parse ("NOSUCH.SYSTEM"), after);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), hr);
        Assert::AreEqual (size_t (0), after.size());
    }

    TEST_METHOD (Volume_SetStartupProgram_TheKernelItself_IsRefusedRatherThanMovedToTheFront)
    {
        // PRODOS is a SYS file like any other and is the one file that must
        // never be nominated: the boot block loads it by name, and a volume
        // whose startup program was the kernel would ask a running ProDOS to
        // load ProDOS. Asserted on a volume where the kernel is the first SYS
        // record, followed by another system program.
        vector<Byte>   disk = MakeVolumeWithKernel();
        ProDosVolume   volume (disk);
        vector<Byte>   after;
        VolumeListing  listing;
        HRESULT        hr   = S_OK;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (0), IndexOfEntry (listing, "PRODOS"),
            L"the kernel is the first record on this disk, which is what makes it the "
            L"one a naive rule would leave alone and a wrong rule would move");

        hr = volume.SetStartupProgram (FilePath::Parse ("PRODOS"), after);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE), hr);
        Assert::AreEqual (size_t (0), after.size());
    }
};
