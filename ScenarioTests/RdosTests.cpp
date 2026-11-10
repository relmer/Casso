#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/RdosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  RdosTests
//
//  CiderPress II's RDOS save disks, read by Casso and compared with the files
//  CiderPress II extracts from them. The disks come from
//  scripts/FetchCiderPressTestFiles.ps1, and its -Extract switch writes
//  CiderPress II's extraction; each test is skipped when either is absent.
//
//  CiderPress II extracts every sector of a T file. Casso ends a T file at its
//  first $00, so a T file is compared up to that byte.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RdosTests)
{
public:

    static constexpr const char *  kRoot = "CiderPress";

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

    static void CompareWithExtraction (const std::string & image, RdosVolume::Layout layout)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::unique_ptr<IVolume>  volume;
        SectorDecodeReport        report;
        fs::path                  extracted = fs::path (fixtures.GetRoot()) / kRoot / "extracted" / fs::path (image).stem();
        size_t                    compared  = 0;
        std::string               mismatches;
        HRESULT                   hrOpen    = S_OK;



        hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/rdos/" + image, bytes);

        if (FAILED (hrOpen))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
            return;
        }

        if (!fs::exists (extracted))
        {
            Logger::WriteMessage ("SKIPPED: CiderPress II's extraction is absent; run scripts/FetchCiderPressTestFiles.ps1 -Extract\n");
            return;
        }

        AssertSucceeded (VolumeImage::Load (bytes, image, sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Rdos);
        Assert::IsTrue (RdosVolume::DetectLayout (sectors) == layout);

        volume = VolumeImage::CreateVolume (VolumeKind::Rdos, sectors);
        Assert::IsNotNull (volume.get());

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

            if (payload.type == RdosVolume::kTypeText)
            {
                reference.erase (std::find (reference.begin(), reference.end(), (Byte) 0), reference.end());
            }

            if (payload.bytes != reference)
            {
                mismatches += std::format ("{}: {} bytes, CiderPress II has {}\n", name, payload.bytes.size(), reference.size());
            }

            if (RdosVolume::GetProDosType (payload.type) != type)
            {
                mismatches += std::format ("{}: type ${:02X}, CiderPress II has ${:02X}\n", name, RdosVolume::GetProDosType (payload.type), type);
            }

            if (payload.type == RdosVolume::kTypeBinary && payload.loadAddress != aux)
            {
                mismatches += std::format ("{}: load address ${:04X}, CiderPress II has ${:04X}\n", name, payload.loadAddress, aux);
            }

            compared++;
        }

        Logger::WriteMessage (std::format ("Compared {} files of {} with CiderPress II's extraction\n", compared, image).c_str());
        Assert::IsTrue (compared > 0);
        Assert::IsTrue (mismatches.empty(), TextEncoding::Utf8ToWide (mismatches).c_str());
    }


    TEST_METHOD (Rdos33SaveDisk_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("rdos33-rw2000-save.woz", RdosVolume::Layout::Rdos33);
    }


    TEST_METHOD (Rdos3SaveDisk_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("rdos3-rss-save.woz", RdosVolume::Layout::Rdos3);
    }


    TEST_METHOD (Rdos32SaveDisk_EveryFileMatchesCiderPressExtraction)
    {
        CompareWithExtraction ("rdos32-shiloh-save.woz", RdosVolume::Layout::Rdos32);
    }
};
