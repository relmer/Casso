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
//  Every file on the fixture disks identified, decoded and exported; each
//  identified file must decode, and decode within half a second at up to
//  64 KB, and a file of an import row must come back from its conversion
//  to the same conversion. CiderPress II's test disks are skipped when
//  scripts/FetchCiderPressTestFiles.ps1 hasn't been run.
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


    //  A conversion put through its row's importer must be the file on the
    //  disk, byte for byte, and must decode to the same conversion.
    static void RoundTrip (const DecodedFile & decoded, FileFormat format, const FileEntry & entry, VolumeKind kind, const std::vector<Byte> & original, const std::string & path, std::string & outFailures, size_t & outRoundTrips)
    {
        std::vector<Byte>  first;
        std::vector<Byte>  second;
        FilePayload        imported;
        DecodedFile        again;
        FileFacts          facts = FileFacts::From (entry, kind);
        std::wstring       error;
        HRESULT            hr    = FileFormatTable::Export (decoded, first);



        if (FAILED (hr))
        {
            outFailures += std::format ("{}: no export for the round trip\n", path);
            return;
        }

        hr = FileFormatTable::Import (format, kind, first, imported, error);

        if (FAILED (hr))
        {
            outFailures += std::format ("{}: import failed: {}\n", path, TextEncoding::WideToUtf8 (error));
            return;
        }

        if (imported.bytes != original)
        {
            outFailures += std::format ("{}: the imported file is not the file on the disk\n", path);
            return;
        }

        hr = FileFormatTable::Decode (format, imported.bytes, facts, DecodeOptions(), again, error);

        if (SUCCEEDED (hr))
        {
            hr = FileFormatTable::Export (again, second);
        }

        if (FAILED (hr) || first != second)
        {
            outFailures += std::format ("{}: the imported file is not the conversion it was made from\n", path);
            return;
        }

        outRoundTrips++;
    }


    static bool HasImport (FileFormat format)
    {
        for (const FileFormatTable::Row & row : FileFormatTable::GetRows())
        {
            if (row.format == format)
            {
                return row.import != nullptr;
            }
        }

        return false;
    }


    static void Walk (IVolume & volume, VolumeKind kind, const std::string & directory, std::string & outLog, std::string & outFailures, size_t & outDecoded, std::set<FileFormat> & outFormats, size_t & outRoundTrips)
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
                Walk (volume, kind, path, outLog, outFailures, outDecoded, outFormats, outRoundTrips);
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
            outFormats.insert (format);
            outDecoded++;

            if (HasImport (format))
            {
                RoundTrip (decoded, format, entry, kind, payload.bytes, path, outFailures, outRoundTrips);
            }
        }
    }


    TEST_METHOD (TestFiles_EveryIdentifiedFileDecodes)
    {
        static const char * const  kImages[] =
        {
            "CiderPress/fileconv/test-files.sdk",
            "CiderPress/fileconv/dos-files.do.gz",
            "Disks/Merlin-proDos2.23.dsk",
            "Disks/Merlin-proProdos2.33-a.dsk",
            "Disks/Merlin-proProdos2.33-b.dsk",
            "CassoExplorer/dos33.dsk",
            "CassoExplorer/prodos.po",
        };

        FixtureProvider       fixtures;
        std::set<FileFormat>  formats;
        std::string           log;
        std::string           failures;
        std::string           missing;
        size_t                decoded    = 0;
        size_t                roundTrips = 0;



        for (const char * image : kImages)
        {
            std::vector<Byte>         bytes;
            std::vector<Byte>         sectors;
            SectorDecodeReport        report;
            std::unique_ptr<IVolume>  volume;
            VolumeKind                kind = VolumeKind::Unknown;
            std::string               leaf = std::filesystem::path (image).filename().string();
            HRESULT                   hr   = fixtures.OpenFixture (image, bytes);

            if (FAILED (hr))
            {
                Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
                continue;
            }

            AssertSucceeded (VolumeImage::Load (bytes, leaf, sectors, report));
            kind   = VolumeImage::DetectFilesystem (sectors);
            volume = VolumeImage::CreateVolume (kind, sectors);
            Assert::IsNotNull (volume.get(), TextEncoding::Utf8ToWide (image).c_str());

            log += std::format ("{}:\n", image);
            Walk (*volume, kind, std::string(), log, failures, decoded, formats, roundTrips);
        }

        for (const FileFormatTable::Row & row : FileFormatTable::GetRows())
        {
            if (!formats.contains (row.format))
            {
                missing += std::format ("  {} ({})\n", (int) row.format, TextEncoding::WideToUtf8 (row.suffix));
            }
        }

        Logger::WriteMessage (log.c_str());
        Logger::WriteMessage (std::format ("Decoded {} files, {} through an import and back\n", decoded, roundTrips).c_str());
        Logger::WriteMessage (std::format ("Formats with no fixture:\n{}", missing).c_str());
        Assert::IsTrue (decoded > 0);
        Assert::IsTrue (roundTrips > 0);
        Assert::IsTrue (failures.empty(), TextEncoding::Utf8ToWide (failures).c_str());
    }
};