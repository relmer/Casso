#include "Pch.h"

#include "Update/ZipArchive.h"
#include "TestZipBuilder.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  A deflated zip: .NET's System.IO.Compression.ZipArchive with one entry,
//  docs/notes.txt, holding "Casso update test. " twelve times, captured once.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte s_kDeflatedZip[] =
{
    0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x36, 0x6C, 0x46, 0x5D, 0x9E, 0x98,
    0xCB, 0xC7, 0x18, 0x00, 0x00, 0x00, 0xE4, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x64, 0x6F,
    0x63, 0x73, 0x2F, 0x6E, 0x6F, 0x74, 0x65, 0x73, 0x2E, 0x74, 0x78, 0x74, 0x73, 0x4E, 0x2C, 0x2E,
    0xCE, 0x57, 0x28, 0x2D, 0x48, 0x49, 0x2C, 0x49, 0x55, 0x28, 0x49, 0x2D, 0x2E, 0xD1, 0x53, 0x70,
    0x1E, 0x6E, 0x42, 0x00, 0x50, 0x4B, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x36, 0x6C, 0x46, 0x5D, 0x9E, 0x98, 0xCB, 0xC7, 0x18, 0x00, 0x00, 0x00, 0xE4, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x6F, 0x63, 0x73, 0x2F, 0x6E, 0x6F, 0x74, 0x65, 0x73, 0x2E, 0x74, 0x78, 0x74,
    0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x3C, 0x00, 0x00, 0x00,
    0x44, 0x00, 0x00, 0x00, 0x00, 0x00
};





////////////////////////////////////////////////////////////////////////////////
//
//  ZipArchiveTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ZipArchiveTests)
{
public:

    static std::string ToString (const std::vector<Byte> & bytes)
    {
        return std::string (bytes.begin(), bytes.end());
    }



    TEST_METHOD (Crc32_KnownValue)
    {
        static constexpr Byte kCheck[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };



        Assert::AreEqual ((std::uint32_t) 0xCBF43926, ZipArchive::ComputeCrc32 (kCheck), L"the standard CRC-32 check value");
        Assert::AreEqual ((std::uint32_t) 0,          ZipArchive::ComputeCrc32 ({}));
    }



    TEST_METHOD (Stored_EntriesAndDirectories)
    {
        TestZipBuilder          builder;
        std::vector<Byte>       zip;
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        builder.Add ("Casso-x64/", "");
        builder.Add ("Casso-x64/Casso.exe", "MZ fake exe");
        builder.Add ("Casso-x64/Demos/hello.dsk", std::string (300, '\x5A'));
        zip = builder.Build();

        hr = ZipArchive::Extract (zip, entries);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 3, entries.size());
        Assert::IsTrue   (entries[0].isDirectory);
        Assert::AreEqual (std::string ("Casso-x64/Casso.exe"), entries[1].path);
        Assert::IsFalse  (entries[1].isDirectory);
        Assert::AreEqual (std::string ("MZ fake exe"), ToString (entries[1].data));
        Assert::AreEqual ((size_t) 300, entries[2].data.size());
    }



    TEST_METHOD (Deflated_CapturedArchive)
    {
        std::vector<ZipEntry>  entries;
        std::string            expected;
        HRESULT                hr       = S_OK;



        for (int i = 0; i < 12; i++)
        {
            expected += "Casso update test. ";
        }

        hr = ZipArchive::Extract (s_kDeflatedZip, entries);
        AssertSucceeded (hr);

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual (std::string ("docs/notes.txt"), entries[0].path);
        Assert::AreEqual (expected, ToString (entries[0].data));
    }



    TEST_METHOD (Empty_ArchiveHasNoEntries)
    {
        TestZipBuilder          builder;
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        hr = ZipArchive::Extract (builder.Build(), entries);
        AssertSucceeded (hr);
        Assert::IsTrue  (entries.empty());
    }



    TEST_METHOD (Rejects_CrcMismatch)
    {
        TestZipBuilder          builder;
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        builder.Add ("a.txt", "abc").crc32    = 0x12345678;
        builder.entries.back().isCrcSet       = true;

        hr = ZipArchive::Extract (builder.Build(), entries);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsTrue   (entries.empty());
    }



    TEST_METHOD (Rejects_EncryptionAndUnknownMethod)
    {
        TestZipBuilder          encrypted;
        TestZipBuilder          bzip2;
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        encrypted.Add ("a.txt", "abc").flags = 1;
        bzip2.Add ("a.txt", "abc").method    = 12;

        hr = ZipArchive::Extract (encrypted.Build(), entries);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"encrypted");

        hr = ZipArchive::Extract (bzip2.Build(), entries);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"method 12");
    }



    TEST_METHOD (Rejects_UnsafePaths)
    {
        static constexpr const char * kUnsafe[] =
        {
            "/etc/passwd", "\\Windows\\x.dll", "C:/x.txt", "a/../../x.txt", "..\\x.txt", "a/..", "",
        };

        std::vector<ZipEntry>  entries;
        HRESULT                hr      = S_OK;



        for (const char * path : kUnsafe)
        {
            TestZipBuilder  builder;

            builder.Add ("ok.txt", "fine");
            builder.Add (path, "bad");

            hr = ZipArchive::Extract (builder.Build(), entries);
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (path, path + strlen (path)).c_str());
            Assert::IsTrue   (entries.empty(), L"one bad entry fails the archive");
        }

        Assert::IsTrue (ZipArchive::IsSafeEntryPath ("a/b..c/d.txt"), L"dots inside a name are fine");
        Assert::IsTrue (ZipArchive::IsSafeEntryPath ("dir/"));
    }



    TEST_METHOD (Rejects_TruncatedAndJunk)
    {
        TestZipBuilder          builder;
        std::vector<Byte>       zip;
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        builder.Add ("a.txt", "abcdef");
        zip = builder.Build();

        for (size_t cut = 1; cut < zip.size(); cut++)
        {
            hr = ZipArchive::Extract (std::span<const Byte> (zip.data(), zip.size() - cut), entries);
            Assert::IsTrue (FAILED (hr), std::format (L"cut {} bytes", cut).c_str());
        }

        hr = ZipArchive::Extract (std::span<const Byte> (s_kDeflatedZip, 4), entries);
        Assert::IsTrue (FAILED (hr));
    }



    TEST_METHOD (Rejects_CorruptDeflateData)
    {
        std::vector<Byte>       zip (std::begin (s_kDeflatedZip), std::end (s_kDeflatedZip));
        std::vector<ZipEntry>   entries;
        HRESULT                 hr      = S_OK;



        zip[44] ^= 0xFF;        // first byte of the deflated data

        hr = ZipArchive::Extract (zip, entries);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }
};
