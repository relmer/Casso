#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileTypeFixtureTests
//
//  Every file on CiderPress II's test disks identified, decoded and
//  exported; each identified file must decode, and decode within half a
//  second at up to 64 KB. Skipped when scripts/FetchCiderPressTestFiles.ps1
//  hasn't been run.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileTypeFixtureTests)
{
public:

    //  Each conversion written to the folder CASSO_FILETYPE_EXPORT has, when
    //  it is set, for a look at what the decoders draw.
    static void Export (const DecodedFile & decoded, const std::string & path, FileFormat format)
    {
        wchar_t            folder[MAX_PATH] = {};
        std::vector<Byte>  bytes;
        std::wstring       name;
        HRESULT            hr               = S_OK;



        if (GetEnvironmentVariableW (L"CASSO_FILETYPE_EXPORT", folder, MAX_PATH) == 0)
        {
            return;
        }

        hr = FileFormatTable::Export (decoded, bytes);

        if (FAILED (hr))
        {
            return;
        }

        name = TextEncoding::Utf8ToWide (path);
        std::replace (name.begin(), name.end(), L'/', L'_');

        std::ofstream  out (std::filesystem::path (folder) / (name + FileFormatTable::GetSuffix (format)), std::ios::binary);

        out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
    }


    static void Walk (IVolume & volume, VolumeKind kind, const std::string & directory, std::string & outLog, std::string & outFailures, size_t & outDecoded)
    {
        VolumeListing  listing;
        HRESULT        hr = directory.empty() ? volume.Enumerate (listing) : volume.EnumerateDirectory (FilePath::Parse (directory), listing);



        if (FAILED (hr))
        {
            return;
        }

        for (const FileEntry & entry : listing.entries)
        {
            std::string   path   = directory.empty() ? entry.name : directory + "/" + entry.name;
            FilePayload   payload;
            DecodedFile   decoded;
            FileFormat    format = FileFormat::None;
            FileFacts     facts;
            std::wstring  error;
            ULONGLONG     start  = 0;
            ULONGLONG     took   = 0;

            if (entry.isDirectory)
            {
                Walk (volume, kind, path, outLog, outFailures, outDecoded);
                continue;
            }

            hr = volume.Read (FilePath::Parse (path), payload);

            if (FAILED (hr))
            {
                continue;
            }

            format = FileFormatTable::Identify (entry, kind, payload.bytes);

            if (format == FileFormat::None)
            {
                continue;
            }

            facts              = FileFacts::From (entry, kind);
            facts.resourceFork = payload.resourceFork;

            start = GetTickCount64();
            hr    = FileFormatTable::Decode (format, payload.bytes, facts, DecodeOptions(), decoded, error);
            took  = GetTickCount64() - start;

            outLog += std::format ("{} -> {} {}\n", path, (int) format, SUCCEEDED (hr) ? "ok" : TextEncoding::WideToUtf8 (error));

            if (FAILED (hr))
            {
                outFailures += std::format ("{}: {}\n", path, TextEncoding::WideToUtf8 (error));
                continue;
            }

            if (payload.bytes.size() <= 65536 && took > 500)
            {
                outFailures += std::format ("{}: took {} ms\n", path, took);
            }

            Export (decoded, path, format);
            outDecoded++;
        }
    }


    TEST_METHOD (TestFiles_EveryIdentifiedFileDecodes)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        SectorDecodeReport        report;
        std::unique_ptr<IVolume>  volume;
        VolumeKind                kind     = VolumeKind::Unknown;
        std::string               log;
        std::string               failures;
        size_t                    decoded  = 0;
        HRESULT                   hr       = fixtures.OpenFixture ("CiderPress/fileconv/test-files.sdk", bytes);



        if (FAILED (hr))
        {
            Logger::WriteMessage ("SKIPPED: test-files.sdk is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n");
            return;
        }

        AssertSucceeded (VolumeImage::Load (bytes, "test-files.sdk", sectors, report));
        kind   = VolumeImage::DetectFilesystem (sectors);
        volume = VolumeImage::CreateVolume (kind, sectors);
        Assert::IsNotNull (volume.get());

        Walk (*volume, kind, std::string(), log, failures, decoded);

        Logger::WriteMessage (log.c_str());
        Logger::WriteMessage (std::format ("Decoded {} files\n", decoded).c_str());
        Assert::IsTrue (decoded > 0);
        Assert::IsTrue (failures.empty(), TextEncoding::Utf8ToWide (failures).c_str());
    }
};
