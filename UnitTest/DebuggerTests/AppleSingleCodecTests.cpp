#include "Pch.h"

#include "Core/AppleSingleCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodecTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleSingleCodecTests)
    {
    public:

        TEST_METHOD (RoundTrip_DataNameAndProDosInfo)
        {
            AppleSingleFile     original;
            AppleSingleFile     decoded;
            std::vector<Byte>   bytes;
            std::string         error;



            original.data          = { 0xA9, 0x41, 0x60 };
            original.realName      = "PROG";
            original.hasProDosInfo = true;
            original.access        = 0x00C3;
            original.fileType      = 0x0006;
            original.auxType       = 0x00000300;

            AppleSingleCodec::Encode (original, bytes);

            Assert::IsTrue   (AppleSingleCodec::IsAppleSingle (bytes));
            Assert::AreEqual ((Byte) 0x00, bytes[0]);
            Assert::AreEqual ((Byte) 0x05, bytes[1]);
            Assert::AreEqual ((Byte) 0x16, bytes[2]);
            Assert::AreEqual ((Byte) 0x00, bytes[3]);
            Assert::AreEqual (S_OK, AppleSingleCodec::Decode (bytes, decoded, error), std::wstring (error.begin(), error.end()).c_str());
            Assert::IsTrue   (decoded.data == original.data);
            Assert::AreEqual (std::string ("PROG"), decoded.realName);
            Assert::IsTrue   (decoded.hasProDosInfo);
            Assert::AreEqual ((Word) 0x0006,        decoded.fileType);
            Assert::AreEqual ((uint32_t) 0x0300,    decoded.auxType);
            Assert::AreEqual ((Word) 0x00C3,        decoded.access);
        }



        TEST_METHOD (DataForkOnly_NoNameNoInfo)
        {
            AppleSingleFile     original;
            AppleSingleFile     decoded;
            std::vector<Byte>   bytes;
            std::string         error;



            original.data = { 1, 2, 3, 4 };
            AppleSingleCodec::Encode (original, bytes);

            Assert::AreEqual ((size_t) 26 + 12 + 4, bytes.size(), L"one entry descriptor and the fork");
            Assert::AreEqual (S_OK, AppleSingleCodec::Decode (bytes, decoded, error));
            Assert::AreEqual ((size_t) 4, decoded.data.size());
            Assert::IsTrue   (decoded.realName.empty());
            Assert::IsFalse  (decoded.hasProDosInfo);
        }



        TEST_METHOD (Truncated_UnknownVersion_NotAppleSingle)
        {
            AppleSingleFile     original;
            AppleSingleFile     decoded;
            std::vector<Byte>   bytes;
            std::vector<Byte>   cut;
            std::string         error;
            HRESULT             hr = S_OK;



            original.data     = { 1, 2, 3, 4, 5, 6, 7, 8 };
            original.realName = "X";
            AppleSingleCodec::Encode (original, bytes);
            Assert::AreEqual ((size_t) 26 + 24 + 8 + 1, bytes.size());

            cut.assign (bytes.begin(), bytes.begin() + 40);
            hr = AppleSingleCodec::Decode (cut, decoded, error);
            Assert::IsTrue   (FAILED (hr), L"an entry table past the end of the file");
            Assert::IsTrue   (error.find ("entry table") != std::string::npos);

            cut.assign (bytes.begin(), bytes.begin() + 55);
            hr = AppleSingleCodec::Decode (cut, decoded, error);
            Assert::IsTrue   (FAILED (hr), L"a fork past the end of the file");
            Assert::IsTrue   (error.find ("entry 1") != std::string::npos, std::wstring (error.begin(), error.end()).c_str());

            bytes[5] = 0x03;
            hr = AppleSingleCodec::Decode (bytes, decoded, error);
            Assert::IsTrue   (FAILED (hr), L"version 3 is not known");
            Assert::IsTrue   (error.find ("version") != std::string::npos);

            bytes[5] = 0x01;
            hr = AppleSingleCodec::Decode (bytes, decoded, error);
            Assert::AreEqual (S_OK, hr, L"version 1 reads the same entries");
            Assert::AreEqual ((size_t) 8, decoded.data.size());

            Assert::IsFalse  (AppleSingleCodec::IsAppleSingle (std::vector<Byte> { 0xA9, 0x41, 0x60 }));
            hr = AppleSingleCodec::Decode (std::vector<Byte> { 0xA9, 0x41, 0x60 }, decoded, error);
            Assert::IsTrue   (FAILED (hr));
        }
    };
}
