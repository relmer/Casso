#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos32Tests
//
//  CiderPress II's DOS 3.2 disks, read by Casso and compared with the files
//  CiderPress II extracts from them. The disks come from
//  scripts/FetchCiderPressTestFiles.ps1, and its -Extract switch writes
//  CiderPress II's extraction; each test is skipped when either is absent.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos32Tests)
{
public:

    static constexpr const char *  kRoot           = "CiderPress";
    static constexpr Byte          kProDosInteger  = 0xFA;
    static constexpr Byte          kProDosBinary   = 0x06;
    static constexpr DWORD         kCp2TimeoutMs   = 60000;

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

    //  A NAPS name, `NAME#TTAAAA`, with `%xx` for a character a PC name can't
    //  have.
    static bool ParseNapsName (const std::string & file, std::string & outName, Byte & outType, Word & outAux)
    {
        size_t  hash = file.rfind ('#');



        if (hash == std::string::npos || file.size() - hash != 7)
        {
            return false;
        }

        outType = (Byte) std::stoul (file.substr (hash + 1, 2), nullptr, 16);
        outAux  = (Word) std::stoul (file.substr (hash + 3, 4), nullptr, 16);
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
        std::string         damaged;
        HRESULT             hrOpen = S_OK;
        int                 track  = 0;



        hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/" + image, bytes);

        if (FAILED (hrOpen))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
            return false;
        }

        AssertSucceeded (VolumeImage::Load (bytes, image, outSectors, report));

        for (track = 0; track < report.GetTrackCount(); track++)
        {
            if (report.GetOutcome (track) != TrackDecodeOutcome::Complete)
            {
                damaged += std::format ("track {} coverage {:04X}\n", track, report.GetCoverage (track));
            }
        }

        Assert::IsTrue (damaged.empty(), TextEncoding::Utf8ToWide (damaged).c_str());

        return true;
    }

    static void CompareWithExtraction (const std::string & image)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        Dos33Geometry             geometry;
        VolumeListing             listing;
        fs::path                  extracted = fs::path (fixtures.GetRoot()) / kRoot / "extracted" / fs::path (image).stem();
        size_t                    compared  = 0;
        std::string               mismatches;



        if (!OpenImage (image, sectors))
        {
            return;
        }

        if (!fs::exists (extracted))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's extraction is absent; run scripts/FetchCiderPressTestFiles.ps1 -Extract\n");
            return;
        }

        Assert::AreEqual (ThirteenSectorCodec::kImageByteSize, sectors.size());
        Assert::IsTrue (Dos33Geometry::TryDetect (sectors, geometry));
        Assert::IsTrue (geometry.IsThirteenSector());
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        Assert::IsNotNull (volume.get());

        AssertSucceeded (volume->Enumerate (listing));

        for (const fs::directory_entry & entry : fs::directory_iterator (extracted))
        {
            std::string        name;
            Byte               type = 0;
            Word               aux  = 0;
            FilePayload        payload;
            std::vector<Byte>  reference;
            std::ifstream      in;
            HRESULT            hr   = S_OK;

            if (!entry.is_regular_file() || !ParseNapsName (TextEncoding::WideToUtf8 (entry.path().filename().wstring()), name, type, aux))
            {
                continue;
            }

            in.open (entry.path(), std::ios::binary);
            reference.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

            hr = volume->Read (FilePath::Parse (name), payload);

            if (FAILED (hr))
            {
                mismatches += std::format ("{}: not read ({:08X})\n", name, (uint32_t) hr);
                continue;
            }

            if (payload.bytes != reference)
            {
                mismatches += std::format ("{}: {} bytes, CiderPress II has {}\n", name, payload.bytes.size(), reference.size());
            }

            if ((type == kProDosInteger) != (payload.type == Dos33Volume::kTypeInteger) || (type == kProDosBinary) != (payload.type == Dos33Volume::kTypeBinary))
            {
                mismatches += std::format ("{}: DOS type ${:02X}, CiderPress II has ProDOS type ${:02X}\n", name, payload.type, type);
            }

            if (payload.type == Dos33Volume::kTypeBinary && payload.loadAddress != aux)
            {
                mismatches += std::format ("{}: load address ${:04X}, CiderPress II has ${:04X}\n", name, payload.loadAddress, aux);
            }

            compared++;
        }

        Logger::WriteMessage (std::format ("Compared {} files of {} with CiderPress II's extraction\n", compared, image).c_str());
        Assert::AreEqual (listing.entries.size(), compared);
        Assert::IsTrue (mismatches.empty(), TextEncoding::Utf8ToWide (mismatches).c_str());
    }

    static void ExpectInitializedVolume (const std::string & image, size_t entries, uint32_t freeSectors)
    {
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        VolumeListing             listing;
        VolumeIntegrityReport     report;



        if (!OpenImage (image, sectors))
        {
            return;
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        Assert::IsNotNull (volume.get());

        AssertSucceeded (volume->Enumerate (listing));
        AssertSucceeded (volume->BuildIntegrityReport (report));

        Assert::AreEqual (entries, listing.entries.size());
        Assert::AreEqual (freeSectors, listing.freeUnits);
        Assert::AreEqual ((uint32_t) (ThirteenSectorCodec::kTrackCount * ThirteenSectorCodec::kSectorsPerTrack), report.GetUnitCount());
    }


    TEST_METHOD (Dos321MasterD13_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("dos32/3.2.1-master.d13");
    }


    TEST_METHOD (Dos321MasterNib_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("nib/dos321-master.nib");
    }


    TEST_METHOD (Dos32MasterWoz_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("woz/dos32master_2.woz");
    }


    TEST_METHOD (Dos32BlankD13_IsAnInitializedDosVolume)
    {
        ExpectInitializedVolume ("dos32/3.2-blank.d13", 0, 403);
    }


    TEST_METHOD (Dos321BlankNib_IsAnInitializedDosVolume)
    {
        ExpectInitializedVolume ("dos32/3.2.1-blank.nib", 1, 401);
    }


    //  A file written to the master disk, saved in the image's format and
    //  loaded again. The disk has three sectors free.
    TEST_METHOD (Dos321MasterNib_FileWrittenAndSaved_ReadsBackFromTheSavedImage)
    {
        static constexpr const char *  kImage = "nib/dos321-master.nib";



        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::vector<Byte>         edited;
        std::vector<Byte>         saved;
        std::vector<Byte>         reloaded;
        std::unique_ptr<IVolume>  volume;
        SectorDecodeReport        report;
        FilePayload               payload;
        FilePayload               back;
        std::string               reason;
        HRESULT                   hrOpen = S_OK;



        hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/" + kImage, bytes);

        if (FAILED (hrOpen))
        {
            Logger::WriteMessage ("SKIPPED: the DOS 3.2.1 master is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n");
            return;
        }

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (200, 0xA5);

        AssertSucceeded (VolumeImage::Load (bytes, kImage, sectors, report));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        Assert::IsNotNull (volume.get());

        AssertSucceeded (volume->Write (FilePath::Parse ("CASSO"), payload, edited));
        AssertSucceeded (VolumeImage::Save (bytes, kImage, edited, saved, reason), TextEncoding::Utf8ToWide (reason).c_str());
        AssertSucceeded (VolumeImage::Load (saved, kImage, reloaded, report));

        Assert::IsFalse (report.HasDataLoss());
        Assert::IsTrue (edited == reloaded);

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, reloaded);
        Assert::IsNotNull (volume.get());

        AssertSucceeded (volume->Read (FilePath::Parse ("CASSO"), back));

        Assert::IsTrue (payload.bytes == back.bytes);
    }


    //  Two files written to the blank disk and the second deleted, saved as a
    //  .d13. CiderPress II's test of the image passes, and the file it
    //  extracts is the one written.
    TEST_METHOD (Dos32BlankD13_ChangedByCasso_PassesCiderPressTest)
    {
        static constexpr const char *  kImage = "dos32/3.2-blank.d13";



        std::vector<Byte>         sectors;
        std::vector<Byte>         first;
        std::vector<Byte>         second;
        std::vector<Byte>         written;
        std::unique_ptr<IVolume>  volume;
        VolumeListing             listing;
        FilePayload               payload;
        fs::path                  folder  = fs::temp_directory_path() / L"CassoDos32Tests";
        fs::path                  file    = folder / L"changed.d13";
        fs::path                  out     = folder / L"extracted";
        fs::path                  back    = out / L"CASSO#062000";
        std::vector<Byte>         extracted;
        std::ifstream             in;
        DWORD                     exit    = 0;



        if (!OpenImage (kImage, sectors))
        {
            return;
        }

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        for (size_t i = 0; i < 700; i++)
        {
            payload.bytes.push_back ((Byte) (i * 13));
        }

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        Assert::IsNotNull (volume.get());
        AssertSucceeded (volume->Write (FilePath::Parse ("CASSO"), payload, first));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, first);
        AssertSucceeded (volume->Write (FilePath::Parse ("SCRATCH"), payload, second));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, second);
        AssertSucceeded (volume->Delete (FilePath::Parse ("SCRATCH"), written));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, written);
        AssertSucceeded (volume->Enumerate (listing));
        Assert::IsTrue (std::none_of (listing.entries.begin(), listing.entries.end(), [] (const FileEntry & entry) { return entry.name == "SCRATCH"; }));

        fs::remove_all (folder);
        fs::create_directories (out);

        {
            std::ofstream  stream (file, std::ios::binary);

            stream.write ((const char *) written.data(), (std::streamsize) written.size());
        }

        if (!RunCp2 (L"test \"" + file.wstring() + L"\"", exit))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's cp2.exe isn't installed\n");
            return;
        }

        Assert::AreEqual ((DWORD) 0, exit, L"cp2.exe found damage in the changed DOS 3.2 disk");

        RunCp2 (L"extract --preserve=naps \"--exdir=" + out.wstring() + L"\" \"" + file.wstring() + L"\"", exit);
        Assert::AreEqual ((DWORD) 0, exit, L"cp2.exe could not extract the changed DOS 3.2 disk");
        Assert::IsTrue   (fs::exists (back), L"CiderPress II lists the file Casso wrote");

        in.open (back, std::ios::binary);
        extracted.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        in.close();

        Assert::IsTrue (extracted == payload.bytes, L"the file CiderPress II extracts is the one written");

        fs::remove_all (folder);
    }
};
