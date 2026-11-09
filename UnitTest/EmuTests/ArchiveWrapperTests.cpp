#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/BinaryTwoArchive.h"
#include "Devices/Disk/Archives/GzipFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveWrapperTests
//
//  Binary II archives and gzip files built here and read back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ArchiveWrapperTests)
{
public:

    TEST_METHOD (BinaryTwo_RoundTripsFilesAndADirectory)
    {
        std::vector<BinaryTwoEntry>  entries (3);
        std::vector<BinaryTwoEntry>  parsed;
        std::vector<Byte>            bytes;
        std::string                  error;



        entries[0].name     = "GAMES";
        entries[0].fileType = BinaryTwoEntry::kTypeDirectory;
        entries[1].name     = "GAMES/HELLO";
        entries[1].fileType = 0xFC;
        entries[1].auxType  = 0x0801;
        entries[1].data.assign (300, 0x41);
        entries[2].name     = "README";
        entries[2].fileType = 0x04;
        entries[2].data.assign (128, 0x42);

        BinaryTwoArchive::Build (entries, bytes);

        Assert::AreEqual ((size_t) (128 + 128 + 384 + 128 + 128), bytes.size());
        Assert::IsTrue (BinaryTwoArchive::IsBinaryTwo (bytes));
        AssertSucceeded (BinaryTwoArchive::Parse (bytes, parsed, error));
        Assert::AreEqual ((size_t) 3, parsed.size());
        Assert::AreEqual (std::string ("GAMES/HELLO"), parsed[1].name);
        Assert::AreEqual ((Word) 0x0801, parsed[1].auxType);
        Assert::IsTrue (parsed[1].data == entries[1].data);
        Assert::IsTrue (parsed[0].data.empty());
        Assert::IsTrue (parsed[2].data == entries[2].data);
    }


    TEST_METHOD (Gzip_RoundTripsAcrossStoredBlocks)
    {
        std::vector<Byte>  data (150'000, 0);
        std::vector<Byte>  bytes;
        std::vector<Byte>  read;
        std::string        name;
        std::string        error;



        for (size_t i = 0; i < data.size(); i++)
        {
            data[i] = (Byte) (i * 13);
        }

        GzipFile::Build (data, "disk.do", bytes);

        Assert::IsTrue (GzipFile::IsGzip (bytes));
        AssertSucceeded (GzipFile::Expand (bytes, read, name, error));
        Assert::IsTrue (read == data);
        Assert::AreEqual (std::string ("disk.do"), name);
    }


    TEST_METHOD (Gzip_RefusesADamagedTrailer)
    {
        std::vector<Byte>  data (100, 7);
        std::vector<Byte>  bytes;
        std::vector<Byte>  read;
        std::string        name;
        std::string        error;



        GzipFile::Build (data, "", bytes);
        bytes[bytes.size() - 8] ^= 1;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), GzipFile::Expand (bytes, read, name, error));
    }
};