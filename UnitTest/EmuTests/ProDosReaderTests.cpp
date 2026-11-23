#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReaderTests
//
//  ProDosReader: missing files, pointers off the volume, and a directory
//  chain that loops.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosReaderTests)
{
public:

    TEST_METHOD (Reader_MissingFileFailsCleanly)
    {
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertFailed (ProDosReader::ExtractFile (vol, "NOSUCH", readBack, fileType, auxType));
        Assert::IsTrue (readBack.empty());
    }


    TEST_METHOD (Reader_KeyPointerOutsideTheVolume_IsRefusedNotFollowed)
    {
        // Block numbers index straight into the sector buffer through
        // GetBlockByteOffset, which multiplies out without checking anything, so a
        // block past the volume does not read wrong data -- it reads past the
        // end of the buffer entirely. Block 60000 lands roughly 30 MB beyond a
        // 140 KB image.
        //
        // This is reachable today, not only through the new volume layer:
        // InstallBoot extracts PRODOS and BASIC.SYSTEM from a downloaded master
        // for bootable-disk creation.
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  payload (600, 0x5A);   // two blocks -- a sapling
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        size_t        entryAt  = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, payload));

        // Point the entry's key block far outside the volume.
        entryAt = ProDosSkeleton::GetBlockByteOffset (2, 0x04 + 0x27 + 0x11);

        vol[entryAt]     = 0x60;
        vol[entryAt + 1] = 0xEA;   // 60000

        AssertFailed (ProDosReader::ExtractFile (vol, "VICTIM", readBack, fileType, auxType));
    }


    TEST_METHOD (Reader_IndexEntryOutsideTheVolume_IsRefusedNotFollowed)
    {
        // The same hazard one level down: the key block is valid but a pointer
        // inside it is not. Gathering it would read outside the buffer, and
        // returning what was gathered would hand back a truncated file that
        // looks whole.
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  payload (600, 0x33);
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        Word          keyBlock = 0;
        size_t        entryAt  = 0;

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "VICTIM", 0x06, 0x2000, payload));

        entryAt  = ProDosSkeleton::GetBlockByteOffset (2, 0x04 + 0x27 + 0x11);
        keyBlock = (Word) (vol[entryAt] | (vol[entryAt + 1] << 8));

        // First data pointer in the index block -> outside the volume.
        vol[ProDosSkeleton::GetBlockByteOffset (keyBlock, 0)]       = 0x60;
        vol[ProDosSkeleton::GetBlockByteOffset (keyBlock, 0 + 256)] = 0xEA;

        AssertFailed (ProDosReader::ExtractFile (vol, "VICTIM", readBack, fileType, auxType));
    }


    TEST_METHOD (Reader_DirectoryChainThatLoops_Terminates)
    {
        // A directory chain pointing back at itself. Without a guard this never
        // returns, which for a recovery tool is the worst failure of all: the
        // user cannot even see what went wrong.
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;

        // Key block 2's next pointer -> itself.
        vol[ProDosSkeleton::GetBlockByteOffset (2, 0x02)]     = 0x02;
        vol[ProDosSkeleton::GetBlockByteOffset (2, 0x02 + 1)] = 0x00;

        AssertFailed (ProDosReader::ExtractFile (vol, "NOSUCH", readBack, fileType, auxType));
    }
};
