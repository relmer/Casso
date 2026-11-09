#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveTests
//
//  CiderPress II's test archives and disks, read by Casso and compared with
//  what CiderPress II reads from them, and archives Casso writes, tested by
//  CiderPress II. The files come from scripts/FetchCiderPressTestFiles.ps1,
//  and its -Extract switch writes CiderPress II's extraction of
//  test-files.sdk; each test is skipped when its files or cp2.exe are absent.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ArchiveTests)
{
public:

    static constexpr const char *  kRoot = "CiderPress";

    static bool TryReadFixture (const std::string & relative, std::vector<Byte> & outBytes)
    {
        FixtureProvider  fixtures;
        HRESULT          hr = fixtures.OpenFixture (std::string (kRoot) + "/" + relative, outBytes);



        if (FAILED (hr))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", relative).c_str());
        }

        return SUCCEEDED (hr);
    }

    static std::unique_ptr<IVolume> OpenVolume (const std::vector<Byte> & bytes, const std::string & name, std::vector<Byte> & outSectors)
    {
        SectorDecodeReport  report;



        AssertSucceeded (VolumeImage::Load (bytes, name, outSectors, report));

        return VolumeImage::CreateVolume (VolumeImage::DetectFilesystem (outSectors), outSectors);
    }

    //  A NAPS name, `NAME#TTAAAA` with `r` after for a resource fork, and
    //  `%xx` for a character a PC name can't have.
    static bool ParseNapsName (const std::string & file, std::string & outName, Byte & outType, Word & outAux, bool & outResource)
    {
        size_t       hash = file.rfind ('#');
        std::string  tail;



        if (hash == std::string::npos)
        {
            return false;
        }

        tail        = file.substr (hash + 1);
        outResource = tail.size() == 7 && tail.back() == 'r';

        if (tail.size() != 6 && !outResource)
        {
            return false;
        }

        outType = (Byte) std::stoul (tail.substr (0, 2), nullptr, 16);
        outAux  = (Word) std::stoul (tail.substr (2, 4), nullptr, 16);
        outName.clear();

        for (size_t i = 0; i < hash; i++)
        {
            if (file[i] == '%' && i + 2 < hash)
            {
                outName += (char) std::stoul (file.substr (i + 1, 2), nullptr, 16);
                i       += 2;
            }
            else
            {
                outName += file[i];
            }
        }

        return true;
    }

    static std::wstring FindCp2()
    {
        wchar_t       buffer[MAX_PATH] = {};
        std::wstring  path;



        if (GetEnvironmentVariableW (L"LOCALAPPDATA", buffer, MAX_PATH) > 0)
        {
            path = std::wstring (buffer) + L"\\Programs\\CiderPress2\\cp2.exe";
        }

        return fs::exists (path) ? path : std::wstring();
    }

    static int RunCp2 (const std::wstring & cp2, const std::wstring & arguments, std::string & outOutput)
    {
        std::wstring  command = L"\"\"" + cp2 + L"\" " + arguments + L" 2>&1\"";
        FILE        * pipe    = _wpopen (command.c_str(), L"r");
        char          line[512];



        outOutput.clear();

        if (pipe == nullptr)
        {
            return -1;
        }

        while (fgets (line, sizeof (line), pipe) != nullptr)
        {
            outOutput += line;
        }

        return _pclose (pipe);
    }


    TEST_METHOD (TestFilesSdk_EveryForkMatchesCiderPressExtraction)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        fs::path                  extracted = fs::path (fixtures.GetRoot()) / kRoot / "extracted" / "test-files";
        size_t                    compared  = 0;
        std::string               mismatches;



        if (!TryReadFixture ("fileconv/test-files.sdk", bytes))
        {
            return;
        }

        if (!fs::exists (extracted))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's extraction is absent; run scripts/FetchCiderPressTestFiles.ps1 -Extract\n");
            return;
        }

        volume = OpenVolume (bytes, "test-files.sdk", sectors);
        Assert::IsNotNull (volume.get());

        for (const fs::directory_entry & entry : fs::recursive_directory_iterator (extracted))
        {
            std::string        name;
            Byte               type     = 0;
            Word               aux      = 0;
            bool               resource = false;
            FilePayload        payload;
            std::vector<Byte>  reference;
            std::string        relative;
            std::ifstream      in;
            HRESULT            hr       = S_OK;

            if (!entry.is_regular_file() || !ParseNapsName (TextEncoding::WideToUtf8 (entry.path().filename().wstring()), name, type, aux, resource))
            {
                continue;
            }

            relative = TextEncoding::WideToUtf8 (fs::relative (entry.path().parent_path(), extracted).generic_wstring());
            relative = (relative == "." || relative.empty()) ? name : relative + "/" + name;

            in.open (entry.path(), std::ios::binary);
            reference.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

            hr = volume->Read (FilePath::Parse (relative), payload);

            if (FAILED (hr))
            {
                mismatches += std::format ("{}: not read ({:08X})\n", relative, (uint32_t) hr);
                continue;
            }

            if ((resource ? payload.resourceFork : payload.bytes) != reference)
            {
                mismatches += std::format ("{}{}: {} bytes, CiderPress II has {}\n", relative, resource ? " (resource fork)" : "",
                                           (resource ? payload.resourceFork : payload.bytes).size(), reference.size());
            }

            if (payload.type != type)
            {
                mismatches += std::format ("{}: type ${:02X}, CiderPress II has ${:02X}\n", relative, payload.type, type);
            }

            compared++;
        }

        Logger::WriteMessage (std::format ("Compared {} forks with CiderPress II's extraction\n", compared).c_str());
        Assert::IsTrue (compared > 0);
        Assert::IsTrue (mismatches.empty(), TextEncoding::Utf8ToWide (mismatches).c_str());
    }


    TEST_METHOD (DosFilesDoGz_OpensAsDos33)
    {
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        VolumeListing             listing;



        if (!TryReadFixture ("fileconv/dos-files.do.gz", bytes))
        {
            return;
        }

        volume = OpenVolume (bytes, "dos-files.do.gz", sectors);
        Assert::IsNotNull (volume.get());
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);
        AssertSucceeded (volume->Enumerate (listing));
        Assert::IsFalse (listing.entries.empty());
    }


    TEST_METHOD (Archives_EveryRecordExpandsWithItsCrc)
    {
        static constexpr const char *  kArchives[] = { "nufx/PatchHFS.shk", "nufx/gshk-empty-forks.shk", "nufx/mislabeled_bny.shk", "nufx/old-archive.shk",
                                                       "nufx/Samples.BXY", "nufx/GSHK11.SEA", "nufx/DIcEd.BSE", "bny/SAMPLE.BQY" };

        std::string  failures;



        for (const char * name : kArchives)
        {
            std::vector<Byte>         bytes;
            std::vector<std::string>  damage;
            VolumeListing             listing;
            std::string               error;
            HRESULT                   hrParse  = S_OK;
            HRESULT                   hrListed = S_OK;

            if (!TryReadFixture (name, bytes))
            {
                return;
            }

            ArchiveVolume  archive (bytes);

            hrParse  = archive.GetParseResult (error);
            hrListed = SUCCEEDED (hrParse) ? archive.Enumerate (listing) : hrParse;

            if (FAILED (hrListed))
            {
                failures += std::format ("{}: {}\n", name, error);
                continue;
            }

            archive.CollectDamage (damage);

            for (const std::string & record : damage)
            {
                failures += std::format ("{}: {} fails its CRC\n", name, record);
            }
        }

        Assert::IsTrue (failures.empty(), TextEncoding::Utf8ToWide (failures).c_str());
    }


    TEST_METHOD (WrittenArchives_PassCiderPressTest)
    {
        static constexpr const char *  kArchives[] = { "nufx/old-archive.shk", "nufx/SIMPLE.DOS.SDK", "nufx/Samples.BXY", "bny/SAMPLE.BQY" };

        std::wstring  cp2 = FindCp2();
        fs::path      folder;
        std::string   failures;



        if (cp2.empty())
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's cp2.exe is not installed\n");
            return;
        }

        folder = fs::temp_directory_path() / std::format ("CassoArchiveTests-{}", GetCurrentProcessId());
        fs::create_directories (folder);

        for (const char * name : kArchives)
        {
            std::vector<Byte>         bytes;
            std::vector<Byte>         sectors;
            std::vector<Byte>         written;
            std::vector<Byte>         saved;
            std::unique_ptr<IVolume>  volume;
            FilePayload               payload;
            std::string               reason;
            std::string               output;
            std::string               leaf   = fs::path (name).filename().string();
            fs::path                  target = folder / leaf;
            int                       exit   = 0;

            if (!TryReadFixture (name, bytes))
            {
                return;
            }

            payload.bytes.resize (20000);

            for (size_t i = 0; i < payload.bytes.size(); i++)
            {
                payload.bytes[i] = (Byte) ((i * 7) % 251);
            }

            payload.type           = ProDosVolume::kTypeBinary;
            payload.loadAddress    = 0x2000;
            payload.hasLoadAddress = true;

            volume = OpenVolume (bytes, leaf, sectors);
            Assert::IsNotNull (volume.get());
            AssertSucceeded (volume->Write (FilePath::Parse ("NEWFILE"), payload, written));
            AssertSucceeded (VolumeImage::Save (bytes, leaf, written, saved, reason));

            {
                std::ofstream  out (target, std::ios::binary);

                out.write ((const char *) saved.data(), (std::streamsize) saved.size());
            }

            exit = RunCp2 (cp2, L"test --show-notes \"" + target.wstring() + L"\"", output);

            if (exit != 0 || output.find ("no failures") == std::string::npos || output.find ("W)") != std::string::npos)
            {
                failures += std::format ("{}:\n{}\n", leaf, output);
            }
        }

        fs::remove_all (folder);
        Assert::IsTrue (failures.empty(), TextEncoding::Utf8ToWide (failures).c_str());
    }
};
