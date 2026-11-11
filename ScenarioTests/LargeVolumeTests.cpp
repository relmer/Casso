#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  LargeVolumeTests
//
//  CiderPress II's 800K, 2IMG and Disk Copy 4.2 images, and a 32 MB volume
//  Casso makes, read and written by Casso and compared with what CiderPress
//  II extracts. The images come from scripts/FetchCiderPressTestFiles.ps1,
//  and its -Extract switch writes CiderPress II's extraction; each test is
//  skipped when either is absent. The 32 MB comparison runs CiderPress II's
//  cp2.exe, and is skipped when it isn't installed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LargeVolumeTests)
{
public:

    static constexpr const char *  kRoot          = "CiderPress";
    static constexpr size_t        k800KBytes     = 819200;
    static constexpr size_t        k32MbBlocks    = 65535;
    static constexpr DWORD         kCp2TimeoutMs  = 60000;

    //  A NAPS name, `NAME#TTAAAA` or `NAME#TTAAAAr` for a resource fork, with
    //  `%xx` for a character a PC name can't have.
    static bool ParseNapsName (const std::string & file, std::string & outName, bool & outIsResource)
    {
        size_t  hash = file.rfind ('#');



        if (hash == std::string::npos)
        {
            return false;
        }

        outIsResource = file.back() == 'r';
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

    static bool OpenImage (const std::string & image, std::vector<Byte> & outSectors)
    {
        FixtureProvider     fixtures;
        std::vector<Byte>   bytes;
        SectorDecodeReport  report;
        HRESULT             hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/" + image, bytes);



        if (FAILED (hrOpen))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
            return false;
        }

        AssertSucceeded (VolumeImage::Load (bytes, image, outSectors, report));

        return true;
    }

    static std::vector<Byte> ReadHostFile (const fs::path & path)
    {
        std::ifstream  in (path, std::ios::binary);



        return std::vector<Byte> (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    }

    //  Every file under `extracted`, each fork read through `volume` and
    //  compared byte for byte.
    static void CompareTree (const IVolume & volume, const fs::path & extracted, const std::string & image)
    {
        size_t       compared   = 0;
        std::string  mismatches;



        for (const fs::directory_entry & entry : fs::recursive_directory_iterator (extracted))
        {
            std::string        leaf;
            std::string        path;
            bool               isResource = false;
            FilePayload        payload;
            std::vector<Byte>  reference;
            HRESULT            hr         = S_OK;

            if (!entry.is_regular_file() || !ParseNapsName (TextEncoding::WideToUtf8 (entry.path().filename().wstring()), leaf, isResource))
            {
                continue;
            }

            for (const fs::path & part : fs::relative (entry.path().parent_path(), extracted))
            {
                if (part != ".")
                {
                    path += TextEncoding::WideToUtf8 (part.wstring()) + "/";
                }
            }

            path     += leaf;
            reference = ReadHostFile (entry.path());

            hr = volume.Read (FilePath::Parse (path), payload);

            if (FAILED (hr))
            {
                mismatches += std::format ("{}: not read ({:08X})\n", path, (uint32_t) hr);
                continue;
            }

            if ((isResource ? payload.resourceFork : payload.bytes) != reference)
            {
                mismatches += std::format ("{}{}: {} bytes, CiderPress II has {}\n", path, isResource ? " (resource fork)" : "",
                                           (isResource ? payload.resourceFork : payload.bytes).size(), reference.size());
            }

            compared++;
        }

        Logger::WriteMessage (std::format ("Compared {} forks of {} with CiderPress II's extraction\n", compared, image).c_str());
        Assert::IsTrue (compared > 0, L"the extraction has no files");
        Assert::IsTrue (mismatches.empty(), TextEncoding::Utf8ToWide (mismatches).c_str());
    }

    static void CompareWithExtraction (const std::string & image)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        fs::path                  extracted = fs::path (fixtures.GetRoot()) / kRoot / "extracted" / fs::path (image).stem();



        if (!OpenImage (image, sectors))
        {
            return;
        }

        if (!fs::exists (extracted))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's extraction is absent; run scripts/FetchCiderPressTestFiles.ps1 -Extract\n");
            return;
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, sectors);
        CompareTree (*volume, extracted, image);
    }

    //  A new folder, a file copied into it and read back, and an integrity
    //  check of the volume after.
    static void CopyInAndOut (std::vector<Byte> & ioSectors)
    {
        ProDosVolume           volume (ioSectors);
        std::vector<Byte>      withFolder;
        std::vector<Byte>      withFile;
        FilePayload            payload;
        FilePayload            back;
        VolumeIntegrityReport  report;



        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        for (size_t i = 0; i < 40000; i++)
        {
            payload.bytes.push_back ((Byte) (i * 7));
        }

        AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("NEW.FOLDER"), withFolder));
        AssertSucceeded (ProDosVolume (withFolder).Write (FilePath::Parse ("NEW.FOLDER/PROGRAM"), payload, withFile));

        ProDosVolume  written (withFile);

        AssertSucceeded (written.Read (FilePath::Parse ("NEW.FOLDER/PROGRAM"), back));
        Assert::IsTrue (back.bytes == payload.bytes);
        Assert::AreEqual ((Word) 0x2000, back.loadAddress);

        AssertSucceeded (written.BuildIntegrityReport (report));
        Assert::IsTrue (report.IsSafeToCommitAfter (report));

        ioSectors = withFile;
    }

    //  cp2.exe run to completion. False when it isn't installed.
    static bool RunCp2 (const std::wstring & arguments, DWORD & outExitCode)
    {
        wchar_t              local[MAX_PATH] = {};
        std::wstring         cp2;
        std::wstring         command;
        STARTUPINFOW         startup         = { sizeof (startup) };
        PROCESS_INFORMATION  process         = {};
        BOOL                 started         = FALSE;



        outExitCode = 0;

        GetEnvironmentVariableW (L"LOCALAPPDATA", local, MAX_PATH);
        cp2 = std::wstring (local) + L"\\Programs\\CiderPress2\\cp2.exe";

        if (!fs::exists (cp2))
        {
            return false;
        }

        command = L"\"" + cp2 + L"\" " + arguments;
        started = CreateProcessW (nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        Assert::IsTrue (started != FALSE, L"cp2.exe did not start");

        WaitForSingleObject (process.hProcess, kCp2TimeoutMs);
        GetExitCodeProcess (process.hProcess, &outExitCode);
        CloseHandle (process.hThread);
        CloseHandle (process.hProcess);

        return true;
    }

    TEST_METHOD (SimpleSparse800K_EachFileMatchesCiderPress)
    {
        CompareWithExtraction ("prodos/simple-sparse.po");
    }

    TEST_METHOD (ProDos2mg_EachForkMatchesCiderPress)
    {
        CompareWithExtraction ("2img/prodos-disk.2mg");
    }

    //  The disk is HFS, which Casso doesn't read yet; its container decodes
    //  to the 800K volume, with the HFS signature in block 2.
    TEST_METHOD (DiskCopy42_DecodesToTheVolume)
    {
        std::vector<Byte>  sectors;



        if (!OpenImage ("diskcopy/Installer Disk 1.image", sectors))
        {
            return;
        }

        Assert::AreEqual (k800KBytes, sectors.size());
        Assert::AreEqual ((Byte) 'B', sectors[2 * 512]);
        Assert::AreEqual ((Byte) 'D', sectors[2 * 512 + 1]);
    }

    TEST_METHOD (Blank800KAndSparse800K_CopyInANewFolderAndOut)
    {
        for (const char * image : { "prodos/blank-800k.po", "prodos/simple-sparse.po" })
        {
            std::vector<Byte>  sectors;

            if (!OpenImage (image, sectors))
            {
                return;
            }

            Assert::AreEqual (k800KBytes, sectors.size());
            CopyInAndOut (sectors);
        }
    }

    //  A 32 MB volume Casso makes and writes to, extracted by CiderPress II.
    TEST_METHOD (Casso32MbVolume_ReadsTheSameInCiderPress)
    {
        std::vector<Byte>  sectors (k32MbBlocks * ProDosSkeleton::kBlockByteSize, 0);
        fs::path           folder = fs::temp_directory_path() / L"CassoLargeVolumeTests";
        fs::path           image  = folder / L"big.po";
        fs::path           out    = folder / L"extracted";
        DWORD              exit   = 0;



        AssertSucceeded (ProDosSkeleton::Write (sectors, "BIG"));
        CopyInAndOut (sectors);

        fs::remove_all (folder);
        fs::create_directories (out);

        {
            std::ofstream  file (image, std::ios::binary);

            file.write ((const char *) sectors.data(), (std::streamsize) sectors.size());
        }

        if (!RunCp2 (L"extract --preserve=naps \"--exdir=" + out.wstring() + L"\" \"" + image.wstring() + L"\"", exit))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's cp2.exe isn't installed\n");
            return;
        }

        Assert::AreEqual ((DWORD) 0, exit, L"cp2.exe could not extract Casso's 32 MB volume");
        CompareTree (ProDosVolume (sectors), out, "a 32 MB volume");

        fs::remove_all (folder);
    }
};
