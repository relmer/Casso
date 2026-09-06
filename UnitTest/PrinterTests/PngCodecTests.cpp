#include "Pch.h"

#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PngCodecTests
//
//  In-memory PNG round-trips over WIC: RGBA fidelity (including channel order),
//  pHYs dpi survival, and lossless 8bpp index-plane preservation. All buffers
//  are in memory -- no files touched -- so this runs anywhere per Test Isolation.
//
////////////////////////////////////////////////////////////////////////////////

namespace PngCodecTests
{
    TEST_CLASS (PngCodecTests)
    {
    public:

        TEST_METHOD_INITIALIZE (InitCom)
        {
            HRESULT   hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
            m_ownsCom = (hr == S_OK || hr == S_FALSE);
        }


        TEST_METHOD_CLEANUP (UninitCom)
        {
            if (m_ownsCom)
            {
                CoUninitialize();
            }
        }


        TEST_METHOD (RgbaRoundTripPreservesPixels)
        {
            RgbaImage      src;
            RgbaImage      back;
            vector<Byte>   png;

            // 2x2 with channel-distinct colors so a R<->B swap would show.
            src.Allocate (2, 2, 0, 0, 0);
            SetPixel (src, 0, 0, 0xFF, 0x00, 0x00, 0xFF);   // red
            SetPixel (src, 1, 0, 0x00, 0x00, 0xFF, 0xFF);   // blue
            SetPixel (src, 0, 1, 0x10, 0x80, 0xF0, 0xFF);   // R<G<B gradient
            SetPixel (src, 1, 1, 0x20, 0x40, 0x60, 0x80);   // partial alpha

            AssertSucceeded (PngCodec::EncodeRgba (src, 576, png));
            Assert::IsTrue (png.size() > 8);
            AssertSucceeded (PngCodec::DecodeRgba (png, back));

            Assert::AreEqual (src.width,  back.width);
            Assert::AreEqual (src.height, back.height);
            for (size_t i = 0; i < src.rgba.size(); i++)
            {
                Assert::AreEqual (src.rgba[i], back.rgba[i]);
            }
        }


        TEST_METHOD (PhysDpiSurvives)
        {
            RgbaImage      src;
            vector<Byte>   png;
            int            dpi = 0;

            src.Allocate (4, 4, 0xFF, 0xFF, 0xFF);
            AssertSucceeded (PngCodec::EncodeRgba (src, 576, png));
            AssertSucceeded (PngCodec::ReadDpi (png, dpi));

            Assert::AreEqual (576, dpi);
        }


        TEST_METHOD (IndexPlaneRoundTripIsLossless)
        {
            vector<Byte>  indices;
            vector<Byte>  back;
            vector<Byte>  png;
            uint32_t      palette[16];
            int           w           = 4;
            int           h           = 4;
            int           i           = 0;
            int           outW        = 0;
            int           outH        = 0;

            for (i = 0; i < 16; i++)
            {
                palette[i] = 0xFF000000u | (uint32_t) (i * 0x101010);   // opaque gray ramp
                indices.push_back ((Byte) i);                          // one cell per index 0..15
            }

            AssertSucceeded (PngCodec::EncodeIndexed (w, h, indices, palette, 16, 576, png));
            AssertSucceeded (PngCodec::DecodeIndexed (png, outW, outH, back));

            Assert::AreEqual (w, outW);
            Assert::AreEqual (h, outH);
            Assert::AreEqual (indices.size(), back.size());
            for (i = 0; i < (int) indices.size(); i++)
            {
                Assert::AreEqual (indices[i], back[i]);
            }
        }


        TEST_METHOD (EmptyInputRejected)
        {
            vector<Byte>   empty;
            RgbaImage      img;

            AssertFailed (PngCodec::DecodeRgba (empty, img));
        }


        // The chunks are read back OUT OF THE PNG BYTES, not through WIC. A
        // write-then-read round trip through one library can agree with itself
        // about a malformed chunk; parsing the container proves what the file
        // actually says, which is what every other tool will read.
        TEST_METHOD (TextChunksAreWrittenToTheFile)
        {
            RgbaImage               src;
            vector<Byte>            png;
            vector<MetadataEntry>   chunks = { { "Software",      "Casso 1.22.0" },
                                               { "Source",        "Apple //e" },
                                               { "Casso Capture", "scene" } };
            vector<MetadataEntry>   back;

            src.Allocate (2, 2, 0, 0, 0);

            AssertSucceeded (PngCodec::EncodeRgba (src, 0, chunks, png));

            back = ReadTextChunks (png);

            Assert::AreEqual ((size_t) 3, back.size());
            AssertEntry (back[0], "Software",      "Casso 1.22.0");
            AssertEntry (back[1], "Source",        "Apple //e");
            AssertEntry (back[2], "Casso Capture", "scene");
        }


        TEST_METHOD (TextChunksSurviveADecodeRoundTrip)
        {
            RgbaImage               src;
            RgbaImage               back;
            vector<Byte>            png;
            vector<MetadataEntry>   chunks = { { "Casso Capture", "raw" } };

            src.Allocate (2, 2, 0, 0, 0);
            SetPixel (src, 0, 0, 0x10, 0x80, 0xF0, 0xFF);

            AssertSucceeded (PngCodec::EncodeRgba (src, 0, chunks, png));
            AssertSucceeded (PngCodec::DecodeRgba (png, back));

            // Metadata must not disturb the pixels it travels with.
            Assert::AreEqual (2, back.width);
            Assert::AreEqual (2, back.height);
            Assert::AreEqual ((int) 0x10, (int) back.GetPixel (0, 0)[0]);
            Assert::AreEqual ((int) 0x80, (int) back.GetPixel (0, 0)[1]);
            Assert::AreEqual ((int) 0xF0, (int) back.GetPixel (0, 0)[2]);
        }


        // The no-metadata overload and an empty chunk list must both produce a
        // file with no tEXt at all -- not one carrying an empty entry.
        TEST_METHOD (NoChunksWritesNoTextAtAll)
        {
            RgbaImage               src;
            vector<Byte>            withoutArg;
            vector<Byte>            withEmptyList;
            vector<MetadataEntry>   none;

            src.Allocate (2, 2, 0, 0, 0);

            AssertSucceeded (PngCodec::EncodeRgba (src, 0, withoutArg));
            AssertSucceeded (PngCodec::EncodeRgba (src, 0, none, withEmptyList));

            Assert::AreEqual ((size_t) 0, ReadTextChunks (withoutArg).size());
            Assert::AreEqual ((size_t) 0, ReadTextChunks (withEmptyList).size());
        }


        TEST_METHOD (KeywordAtTheLengthLimitIsAccepted)
        {
            RgbaImage               src;
            vector<Byte>            png;
            string                  longest (PngMetadata::kMaxKeywordLength, 'K');
            vector<MetadataEntry>   chunks = { { longest, "value" } };
            vector<MetadataEntry>   back;

            src.Allocate (2, 2, 0, 0, 0);

            AssertSucceeded (PngCodec::EncodeRgba (src, 0, chunks, png));

            back = ReadTextChunks (png);

            Assert::AreEqual ((size_t) 1, back.size());
            AssertEntry (back[0], longest.c_str(), "value");
        }


        TEST_METHOD (ValuesMayContainSpacesAndPunctuation)
        {
            RgbaImage               src;
            vector<Byte>            png;
            string                  pose   = "yaw 12.5  pitch -8.0  zoom 1.00  pan 0.000 0.000";
            vector<MetadataEntry>   chunks = { { "Casso Scene Pose", pose } };
            vector<MetadataEntry>   back;

            src.Allocate (2, 2, 0, 0, 0);

            AssertSucceeded (PngCodec::EncodeRgba (src, 0, chunks, png));

            back = ReadTextChunks (png);

            Assert::AreEqual ((size_t) 1, back.size());
            AssertEntry (back[0], "Casso Scene Pose", pose.c_str());
        }


    private:
        bool   m_ownsCom = false;

        static void AssertEntry (const MetadataEntry & entry, const char * keyword, const char * value)
        {
            Assert::AreEqual (string (keyword), entry.keyword);
            Assert::AreEqual (string (value),   entry.value);
        }


        // Walk the PNG chunk stream and collect every tEXt payload, which is
        // "keyword\0value". Deliberately independent of WIC.
        static vector<MetadataEntry> ReadTextChunks (const vector<Byte> & png)
        {
            constexpr size_t        kSignature   = 8;
            constexpr size_t        kLengthBytes = 4;
            constexpr size_t        kTypeBytes   = 4;
            constexpr size_t        kCrcBytes    = 4;

            vector<MetadataEntry>   found;
            size_t                  pos    = kSignature;
            size_t                  length = 0;
            size_t                  split  = 0;
            string                  type;
            string                  data;

            while (pos + kLengthBytes + kTypeBytes + kCrcBytes <= png.size())
            {
                length = ((size_t) png[pos]     << 24) | ((size_t) png[pos + 1] << 16)
                       | ((size_t) png[pos + 2] <<  8) | ((size_t) png[pos + 3]);

                type.assign ((const char *) &png[pos + kLengthBytes], kTypeBytes);
                pos += kLengthBytes + kTypeBytes;

                if (pos + length + kCrcBytes > png.size())
                {
                    break;
                }

                if (type == "tEXt")
                {
                    data.assign ((const char *) &png[pos], length);
                    split = data.find ('\0');

                    if (split != string::npos)
                    {
                        found.push_back ({ data.substr (0, split), data.substr (split + 1) });
                    }
                }

                pos += length + kCrcBytes;
            }

            return found;
        }

        static void SetPixel (RgbaImage & img, int x, int y, Byte r, Byte g, Byte b, Byte a)
        {
            Byte *   p = img.GetPixel (x, y);
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        }
    };
}
