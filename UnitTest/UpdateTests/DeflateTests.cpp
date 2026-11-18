#include "Pch.h"

#include "Update/Deflate.h"
#include "Update/Inflate.h"
#include "Devices/Disk/Archives/GzipFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeflateTests
//
//  Each stream Deflate writes inflates back to its data: an empty buffer, a
//  disk image that is mostly zeros, text with repeats, and bytes with no
//  pattern, which are stored rather than coded. A gzip file built from an
//  800K image is far smaller than the image.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeflateTests)
{
public:

    static std::vector<Byte> RoundTrip (const std::vector<Byte> & data)
    {
        std::vector<Byte>  deflated;
        std::vector<Byte>  inflated;



        Deflate::Compress (data, deflated);
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
        Assert::IsTrue   (gzip.size() < image.size() / 100, std::format (L"{} bytes", gzip.size()).c_str());
    }
};
