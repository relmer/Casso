#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/NuFxArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NuFxArchiveTests
//
//  Archives built here and read back: file records with a data and a
//  resource fork, a disk record, and the CRCs that mark damage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NuFxArchiveTests)
{
public:

    static std::vector<Byte> MakeBytes (size_t count, Byte step)
    {
        std::vector<Byte>  bytes (count, 0);



        for (size_t i = 0; i < count; i++)
        {
            bytes[i] = (Byte) ((i / 7) * step);
        }

        return bytes;
    }

    static NuFxRecord MakeFile (const std::string & name, const std::vector<Byte> & data, const std::vector<Byte> * fork)
    {
        NuFxRecord  record;



        record.fileType  = 0x06;
        record.extraType = 0x2000;
        record.threads.push_back (NuFxArchive::MakeFilenameThread (name));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, data));

        if (fork != nullptr)
        {
            record.storageType = 5;
            record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindResource, *fork));
        }

        return record;
    }


    TEST_METHOD (Build_AndParse_RoundTripTwoFiles)
    {
        NuFxArchive        archive;
        NuFxArchive        parsed;
        std::vector<Byte>  bytes;
        std::vector<Byte>  data   = MakeBytes (30'000, 3);
        std::vector<Byte>  fork   = MakeBytes (500, 5);
        std::vector<Byte>  read;
        std::string        error;



        archive.records.push_back (MakeFile ("GAMES/HELLO", data, nullptr));
        archive.records.push_back (MakeFile ("DOC", data, &fork));
        archive.Build (bytes);

        Assert::IsTrue (NuFxArchive::IsNuFx (bytes));
        AssertSucceeded (NuFxArchive::Parse (bytes, parsed, error));
        Assert::AreEqual ((size_t) 2, parsed.records.size());
        Assert::AreEqual (std::string ("GAMES/HELLO"), NuFxArchive::GetName (parsed.records[0]));
        Assert::IsTrue (parsed.records[0].headerCrcOk);

        AssertSucceeded (NuFxArchive::ExpandThread (parsed.records[1], *NuFxArchive::FindThread (parsed.records[1], NuFxThread::kClassData, NuFxThread::kKindDataFork), read, error));
        Assert::IsTrue (read == data);

        AssertSucceeded (NuFxArchive::ExpandThread (parsed.records[1], *NuFxArchive::FindThread (parsed.records[1], NuFxThread::kClassData, NuFxThread::kKindResource), read, error));
        Assert::IsTrue (read == fork);
    }


    TEST_METHOD (DiskRecord_ExpandsToItsBlocks)
    {
        NuFxArchive        archive;
        NuFxArchive        parsed;
        NuFxRecord         record;
        std::vector<Byte>  bytes;
        std::vector<Byte>  disk   = MakeBytes (280 * 512, 1);
        std::vector<Byte>  read;
        std::string        error;



        record.extraType   = 280;
        record.storageType = 512;
        record.threads.push_back (NuFxArchive::MakeFilenameThread ("DISK"));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDiskImage, disk));
        record.threads.back().eof = 0;
        archive.records.push_back (record);
        archive.Build (bytes);

        AssertSucceeded (NuFxArchive::Parse (bytes, parsed, error));
        AssertSucceeded (NuFxArchive::ExpandThread (parsed.records[0], parsed.records[0].threads[1], read, error));
        Assert::IsTrue (read == disk);
    }


    TEST_METHOD (DamagedThread_FailsItsCrc)
    {
        NuFxArchive        archive;
        NuFxArchive        parsed;
        std::vector<Byte>  bytes;
        std::vector<Byte>  data   = MakeBytes (100, 9);
        std::vector<Byte>  read;
        std::string        error;



        archive.records.push_back (MakeFile ("A", data, nullptr));
        archive.records[0].threads[1].format = NuFxThread::kFormatStored;
        archive.records[0].threads[1].stored = data;
        archive.Build (bytes);
        bytes[bytes.size() - 3] ^= 0xFF;

        AssertSucceeded (NuFxArchive::Parse (bytes, parsed, error));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), NuFxArchive::ExpandThread (parsed.records[0], parsed.records[0].threads[1], read, error));
    }


    TEST_METHOD (DamagedMasterHeader_IsRefused)
    {
        NuFxArchive        archive;
        NuFxArchive        parsed;
        std::vector<Byte>  bytes;
        std::string        error;



        archive.Build (bytes);
        bytes[10] ^= 1;

        AssertFailed (NuFxArchive::Parse (bytes, parsed, error));
        Assert::IsFalse (error.empty());
    }
};