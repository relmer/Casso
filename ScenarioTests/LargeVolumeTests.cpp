#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/HfsWriter.h"
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

    static void CompareWithExtraction (const std::string & image, VolumeKind kind = VolumeKind::ProDos)
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

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == kind);

        volume = VolumeImage::CreateVolume (kind, sectors);
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

    //  An HFS disk in a Disk Copy 4.2 container.
    TEST_METHOD (DiskCopy42Hfs_EachForkMatchesCiderPress)
    {
        CompareWithExtraction ("diskcopy/Installer Disk 1.image", VolumeKind::Hfs);
    }

    TEST_METHOD (Pascal800K_EachFileMatchesCiderPress)
    {
        CompareWithExtraction ("pascal/Apple Pascal 1.3.po.gz", VolumeKind::Pascal);
    }

    TEST_METHOD (GsOsHfs800K_EachForkMatchesCiderPress)
    {
        CompareWithExtraction ("hfs/simple-gsos.po", VolumeKind::Hfs);
    }

    //  An HFS volume over 1 MB is listed and read by the block: the GS/OS
    //  disk with space after it, in a .po.
    TEST_METHOD (GsOsHfsPastOneMegabyte_IsListedAndReadByTheBlock)
    {
        FixtureProvider     fixtures;
        FakeDiskFileIo      io;
        DiskOperations      ops (io);
        std::vector<Byte>   bytes;
        VolumeListing       listing;
        VolumeKind          kind = VolumeKind::Unknown;
        FilePayload         back;
        HRESULT             hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/hfs/simple-gsos.po", bytes);



        if (FAILED (hrOpen))
        {
            Logger::WriteMessage ("SKIPPED: hfs/simple-gsos.po is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n");
            return;
        }

        bytes.resize (4 * 1024 * 1024, 0);

        io.files["C:\\gsos.po"]  = bytes;
        io.stamps["C:\\gsos.po"] = FileStamp { bytes.size(), 1 };

        AssertSucceeded (ops.List ("C:\\gsos.po", listing, kind).hr);

        Assert::IsTrue   (kind == VolumeKind::Hfs);
        Assert::IsTrue   (!listing.entries.empty());
        Assert::AreEqual (0, io.wholeReads);
        Assert::IsTrue   (io.bytesReadAt < (uint64_t) 512 * 1024);

        AssertSucceeded (ops.Read ("C:\\gsos.po", "SIZES/L131073", back, 0).hr);
        Assert::AreEqual ((size_t) 131073, back.bytes.size());
        Assert::AreEqual (0, io.wholeReads);
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

    //  The image after `change`, which must succeed.
    template <typename Change>
    static std::vector<Byte> ApplyHfs (const std::vector<Byte> & image, Change change)
    {
        HfsVolume          volume (image);
        std::vector<Byte>  written;



        AssertSucceeded (change (volume, written));

        return written;
    }

    //  Folders, files of many sizes, two with resource forks, one renamed and
    //  one deleted, written through HfsVolume.
    static void FillHfs (std::vector<Byte> & ioImage)
    {
        DeleteOutcome  outcome;



        ioImage = ApplyHfs (ioImage, [] (HfsVolume & v, std::vector<Byte> & out) { return v.CreateDirectory (FilePath::Parse ("Games"), out); });
        ioImage = ApplyHfs (ioImage, [] (HfsVolume & v, std::vector<Byte> & out) { return v.CreateDirectory (FilePath::Parse ("Games/Arcade"), out); });

        for (size_t i = 0; i < 40; i++)
        {
            FilePayload  payload;
            std::string  path = std::format ("{}File {}", (i % 3 == 0) ? "Games/" : (i % 3 == 1) ? "Games/Arcade/" : "", i);

            payload.type       = (Byte) (i % 2 == 0 ? 0x06 : 0x04);
            payload.auxType    = 0x2000;
            payload.hasAuxType = true;

            for (size_t b = 0; b < i * 197 + 1; b++)
            {
                payload.bytes.push_back ((Byte) (b * 13 + i));
            }

            if (i % 17 == 5)
            {
                payload.resourceFork.assign (1500 + i, (Byte) i);
                payload.hasResourceFork = true;
            }

            ioImage = ApplyHfs (ioImage, [&] (HfsVolume & v, std::vector<Byte> & out) { return v.Write (FilePath::Parse (path), payload, out); });
        }

        ioImage = ApplyHfs (ioImage, [] (HfsVolume & v, std::vector<Byte> & out) { return v.Rename (FilePath::Parse ("File 2"), "Caf\xC3\xA9 menu", out); });
        ioImage = ApplyHfs (ioImage, [&] (HfsVolume & v, std::vector<Byte> & out) { return v.Delete (FilePath::Parse ("File 5"), out, outcome); });
    }

    //  CiderPress II's test of the image passes, and each fork it extracts
    //  matches what Casso reads.
    static void CheckHfsInCiderPress (const std::vector<Byte> & image, const std::wstring & name)
    {
        fs::path  folder = fs::temp_directory_path() / L"CassoHfsTests";
        fs::path  file   = folder / (name + L".po");
        fs::path  out    = folder / (name + L"-extracted");
        DWORD     exit   = 0;



        fs::create_directories (out);

        {
            std::ofstream  stream (file, std::ios::binary);

            stream.write ((const char *) image.data(), (std::streamsize) image.size());
        }

        if (!RunCp2 (L"test \"" + file.wstring() + L"\"", exit))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's cp2.exe isn't installed\n");
            return;
        }

        Assert::AreEqual ((DWORD) 0, exit, (L"cp2.exe found damage in " + name).c_str());

        RunCp2 (L"extract --preserve=naps \"--exdir=" + out.wstring() + L"\" \"" + file.wstring() + L"\"", exit);
        Assert::AreEqual ((DWORD) 0, exit, (L"cp2.exe could not extract " + name).c_str());

        CompareTree (HfsVolume (image), out, TextEncoding::WideToUtf8 (name));

        fs::remove_all (folder);
    }

    //  HFS volumes Casso formats and writes, 800K and 64 MB, checked and
    //  extracted by CiderPress II.
    TEST_METHOD (CassoHfsVolumes_ReadTheSameInCiderPress)
    {
        for (size_t sectors : { (size_t) 1600, (size_t) 131072 })
        {
            std::vector<Byte>  image;

            AssertSucceeded (HfsWriter::Format (image, sectors, "Casso HFS"));
            FillHfs (image);
            CheckHfsInCiderPress (image, std::format (L"casso-{}", sectors));
        }
    }

    //  A GS/OS HFS disk Casso writes to, checked and extracted by CiderPress
    //  II.
    TEST_METHOD (GsOsHfs800K_WrittenByCassoReadsInCiderPress)
    {
        std::vector<Byte>  image;



        if (!OpenImage ("hfs/simple-gsos.po", image))
        {
            return;
        }

        FillHfs (image);
        CheckHfsInCiderPress (image, L"gsos");
    }

    //  Each catalog's records are in the order HfsBTree sorts them in, so a
    //  tree Casso builds again is in the order GS/OS and the Finder built it.
    TEST_METHOD (HfsCatalogs_AreInTheOrderCassoSorts)
    {
        for (const char * name : { "hfs/simple-gsos.po", "diskcopy/Installer Disk 1.image" })
        {
            std::vector<Byte>              image;
            std::vector<HfsBTree::Record>  records;

            if (!OpenImage (name, image))
            {
                return;
            }

            AssertSucceeded (HfsWriter::ReadCatalogRecords (image, records));
            Assert::IsTrue (records.size() > 10);

            for (size_t i = 1; i < records.size(); i++)
            {
                int  order = HfsBTree::CompareCatalogKeys (records[i - 1].key, records[i].key);

                Assert::IsTrue (order < 0, TextEncoding::Utf8ToWide (std::format ("{}: record {} sorts before record {}", name, i, i - 1)).c_str());
            }
        }
    }
};
