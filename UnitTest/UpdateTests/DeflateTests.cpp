#include "Pch.h"

#include "Update/Deflate.h"
#include "Update/Inflate.h"
#include "Devices/Disk/Archives/GzipFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeflateTests
//
//  Each stream Deflate writes inflates back to its data, at every level: an
//  empty buffer, a disk image that is mostly zeros, text with repeats, bytes
//  with no pattern, which are stored rather than coded, and bytes whose
//  counts are far apart, over many blocks. Text is written in a dynamic
//  block. A gzip file built from an 800K image is far smaller than the
//  image, and its header indicates the level it was written at.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeflateTests)
{
public:

    static std::vector<Byte> RoundTrip (const std::vector<Byte> & data, int level = Deflate::kDefault)
    {
        std::vector<Byte>  deflated;
        std::vector<Byte>  inflated;



        Deflate::Compress (data, deflated, level);
        AssertSucceeded (Inflate::Decompress (deflated, data.size(), inflated));
        Assert::IsTrue (inflated == data, L"the data inflates back unchanged");

        return deflated;
    }


    TEST_METHOD (EmptyData_RoundTrips)
    {
        RoundTrip ({});
    }


    TEST_METHOD (MostlyZeroImage_RoundTripsAndShrinks)
    {
        std::vector<Byte>  image (800 * 1024, 0);
        std::vector<Byte>  deflated;



        for (size_t i = 0; i < 6000; i++)
        {
            image[1024 + i] = (Byte) (i * 7 + (i >> 5));
        }

        deflated = RoundTrip (image);

        Assert::IsTrue (deflated.size() < image.size() / 50, std::format (L"{} bytes", deflated.size()).c_str());
    }


    TEST_METHOD (RepeatedText_RoundTripsAndShrinks)
    {
        std::string        text;
        std::vector<Byte>  data;
        std::vector<Byte>  deflated;



        for (int i = 0; i < 400; i++)
        {
            text += std::format ("Line {} of the catalog, PRODOS SYSTEM FILE {}\r", i, i % 17);
        }

        data.assign (text.begin(), text.end());
        deflated = RoundTrip (data);

        Assert::IsTrue (deflated.size() < data.size() / 3);
    }


    TEST_METHOD (RepeatedText_IsWrittenInADynamicBlock)
    {
        std::string        text;
        std::vector<Byte>  data;
        std::vector<Byte>  deflated;



        for (int i = 0; i < 400; i++)
        {
            text += std::format ("Line {} of the catalog, PRODOS SYSTEM FILE {}\r", i, i % 17);
        }

        data.assign (text.begin(), text.end());
        deflated = RoundTrip (data);

        Assert::AreEqual ((Byte) 2, (Byte) ((deflated[0] >> 1) & 3), L"the first block has dynamic codes");
    }


    TEST_METHOD (EveryLevel_RoundTrips)
    {
        std::vector<Byte>  data;
        uint32_t           seed = 99;
        std::string        text = "SAVE HELLO,S6,D1 CATALOG RUN STARTUP ";



        for (int i = 0; i < 3000; i++)
        {
            data.insert (data.end(), text.begin(), text.end());
            seed = seed * 1103515245 + 12345;
            data.insert (data.end(), (size_t) (seed >> 28), (Byte) (seed >> 16));
            data.push_back ((Byte) (seed >> 8));
        }

        data.insert (data.end(), 70000, 0);

        for (int level = Deflate::kFastest; level <= Deflate::kBest; level++)
        {
            RoundTrip (data, level);
        }
    }


    //  Byte k appears in proportion to the k-th Fibonacci number, shuffled,
    //  so the plain Huffman code of a block is deeper than 15 bits.
    TEST_METHOD (FarApartCounts_RoundTripOverManyBlocks)
    {
        std::vector<Byte>  data;
        uint64_t           a    = 1;
        uint64_t           b    = 1;
        uint64_t           c    = 0;
        uint32_t           seed = 7;
        size_t             j    = 0;



        for (int k = 0; k < 24; k++)
        {
            data.insert (data.end(), (size_t) a, (Byte) k);
            c = a + b;
            a = b;
            b = c;
        }

        for (size_t i = data.size() - 1; i > 0; i--)
        {
            seed = seed * 1103515245 + 12345;
            j    = (seed >> 8) % (i + 1);
            std::swap (data[i], data[j]);
        }

        for (int level = Deflate::kFastest; level <= Deflate::kBest; level += 4)
        {
            RoundTrip (data, level);
        }
    }


    TEST_METHOD (BytesWithNoPattern_AreStored)
    {
        std::vector<Byte>  data (100000);
        std::vector<Byte>  deflated;
        uint32_t           seed = 12345;



        for (Byte & b : data)
        {
            seed = seed * 1103515245 + 12345;
            b    = (Byte) (seed >> 16);
        }

        deflated = RoundTrip (data);

        Assert::AreEqual ((Byte) 0, (Byte) (deflated[0] & 0x6), L"the first block is stored");
    }


    TEST_METHOD (GzipOfAnImage_IsSmallAndExpandsBack)
    {
        std::vector<Byte>  image (800 * 1024, 0);
        std::vector<Byte>  gzip;
        std::vector<Byte>  expanded;
        std::string        name;
        std::string        error;



        image[3000] = 0x5A;

        GzipFile::Build (image, "disk.po", gzip);
        AssertSucceeded (GzipFile::Expand (gzip, expanded, name, error));

        Assert::IsTrue   (expanded == image);
        Assert::AreEqual (std::string ("disk.po"), name);
        Assert::IsTrue   (gzip.size() < 1200, std::format (L"{} bytes", gzip.size()).c_str());
        Assert::AreEqual (Deflate::kDefault, GzipFile::GetLevel (gzip));

        GzipFile::Build (image, "disk.po", gzip, Deflate::kBest);
        Assert::AreEqual (Deflate::kBest, GzipFile::GetLevel (gzip));

        GzipFile::Build (image, "disk.po", gzip, Deflate::kFastest);
        Assert::AreEqual (Deflate::kFastest, GzipFile::GetLevel (gzip));
        AssertSucceeded  (GzipFile::Expand (gzip, expanded, name, error));
        Assert::IsTrue   (expanded == image);
    }
};
