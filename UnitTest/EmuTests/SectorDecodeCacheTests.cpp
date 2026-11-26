#include "Pch.h"

#include "Machines/Apple2/Common/SectorDecodeCache.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeCacheTests
//
//  The cache by file contents, with made-up bytes and sectors.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SectorDecodeCacheTests)
{
public:

    static vector<Byte> MakeFile (Byte seed)
    {
        vector<Byte>  bytes (1001);



        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (seed + i * 7);
        }

        return bytes;
    }



    TEST_METHOD (TheSameBytes_ReturnTheStoredDecode)
    {
        vector<Byte>        file    = MakeFile (1);
        vector<Byte>        sectors (16, 0xA5);
        vector<Byte>        found;
        SectorDecodeReport  report;
        HRESULT             hr      = E_FAIL;
        bool                isFound = false;



        SectorDecodeCache::Store (file, S_OK, sectors, report);
        isFound = SectorDecodeCache::Find (file, hr, found, report);

        Assert::IsTrue   (isFound);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (found == sectors);
    }



    TEST_METHOD (AChangedByte_IsNotFound)
    {
        vector<Byte>        file    = MakeFile (2);
        vector<Byte>        sectors (16, 0x5A);
        vector<Byte>        found;
        SectorDecodeReport  report;
        HRESULT             hr      = S_OK;
        bool                isFound = false;



        SectorDecodeCache::Store (file, S_OK, sectors, report);
        file[500] ^= 0x01;
        isFound = SectorDecodeCache::Find (file, hr, found, report);

        Assert::IsFalse (isFound);
    }



    TEST_METHOD (AFailedDecode_IsReturnedAsFailed)
    {
        vector<Byte>        file    = MakeFile (3);
        vector<Byte>        found;
        SectorDecodeReport  report;
        HRESULT             hr      = S_OK;
        bool                isFound = false;



        SectorDecodeCache::Store (file, E_FAIL, {}, report);
        isFound = SectorDecodeCache::Find (file, hr, found, report);

        Assert::IsTrue   (isFound);
        Assert::AreEqual (E_FAIL, hr);
    }
};
