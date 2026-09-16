#include "Pch.h"

#include "Core/AppleSingleCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSingleCodecTests
//
////////////////////////////////////////////////////////////////////////////////

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



    //  Four dates, one of them unknown, and one before 2000: each comes back as
    //  written, and the unknown one is stored as $80000000.
    TEST_METHOD (RoundTrip_FileDates)
    {
        AppleSingleFile          original;
        AppleSingleFile          decoded;
        std::vector<Byte>        bytes;
        std::string              error;
        static constexpr size_t  kBackupAt = 26 + 2 * 12 + 1 + 8;   // two descriptors, the fork, then create and modify



        original.data       = { 0x60 };
        original.createDate = 0;
        original.modifyDate = -86400;
        original.accessDate = 0x12345678;

        AppleSingleCodec::Encode (original, bytes);

        Assert::AreEqual ((size_t) 26 + 2 * 12 + 1 + 16, bytes.size(), L"a dates descriptor and its 16 bytes");
        Assert::AreEqual ((Byte) 0x80, bytes[kBackupAt]);
        Assert::AreEqual ((Byte) 0x00, bytes[kBackupAt + 3]);

        Assert::AreEqual (S_OK, AppleSingleCodec::Decode (bytes, decoded, error), std::wstring (error.begin(), error.end()).c_str());
        Assert::IsTrue   (decoded.createDate == std::optional<int32_t> (0));
        Assert::IsTrue   (decoded.modifyDate == std::optional<int32_t> (-86400));
        Assert::IsFalse  (decoded.backupDate.has_value(), L"unknown");
        Assert::IsTrue   (decoded.accessDate == std::optional<int32_t> (0x12345678));
    }



    //  Any one date is enough to write the entry.
    TEST_METHOD (OneDateAlone_WritesTheEntry)
    {
        AppleSingleFile     original;
        AppleSingleFile     decoded;
        std::vector<Byte>   bytes;
        std::string         error;



        original.data       = { 0x60 };
        original.accessDate = 7;
        AppleSingleCodec::Encode (original, bytes);

        Assert::AreEqual (S_OK, AppleSingleCodec::Decode (bytes, decoded, error));
        Assert::IsTrue   (decoded.accessDate == std::optional<int32_t> (7));
        Assert::IsFalse  (decoded.createDate.has_value());
    }


    TEST_METHOD (NoDates_NoEntryAndEmptyFields)
    {
        AppleSingleFile     original;
        AppleSingleFile     decoded;
        std::vector<Byte>   bytes;
        std::string         error;



        original.data     = { 1, 2 };
        original.realName = "A";
        AppleSingleCodec::Encode (original, bytes);

        Assert::AreEqual ((size_t) 26 + 2 * 12 + 2 + 1, bytes.size(), L"no dates entry");
        Assert::AreEqual (S_OK, AppleSingleCodec::Decode (bytes, decoded, error));
        Assert::IsFalse  (decoded.createDate.has_value());
        Assert::IsFalse  (decoded.modifyDate.has_value());
        Assert::IsFalse  (decoded.backupDate.has_value());
        Assert::IsFalse  (decoded.accessDate.has_value());
    }



    //  A dates entry shorter than 16 bytes, and one that runs past the end of
    //  the file, are both errors.
    TEST_METHOD (MalformedDatesEntry)
    {
        AppleSingleFile          original;
        AppleSingleFile          decoded;
        std::vector<Byte>        bytes;
        std::vector<Byte>        shortEntry;
        std::vector<Byte>        cut;
        std::string              error;
        HRESULT                  hr              = S_OK;
        static constexpr size_t  kDatesLengthLow = 26 + 12 + 11;   // second descriptor's length, low byte



        original.data       = { 0x60 };
        original.modifyDate = 1;
        AppleSingleCodec::Encode (original, bytes);

        shortEntry = bytes;
        shortEntry[kDatesLengthLow] = 12;
        hr = AppleSingleCodec::Decode (shortEntry, decoded, error);
        Assert::IsTrue (FAILED (hr), L"12 bytes of dates");
        Assert::IsTrue (error.find ("dates") != std::string::npos, std::wstring (error.begin(), error.end()).c_str());

        cut.assign (bytes.begin(), bytes.end() - 4);
        hr = AppleSingleCodec::Decode (cut, decoded, error);
        Assert::IsTrue (FAILED (hr), L"dates past the end of the file");
        Assert::IsTrue (error.find ("entry 8") != std::string::npos, std::wstring (error.begin(), error.end()).c_str());
    }
};
