#include "Pch.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "../Casso/resource.h"
#include "../Casso/EmbeddedMachineConfigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AssetBootstrapTests
//
//  Hermetic per-machine ROM-list verification. The in-app downloader
//  derives its set of ROMs to fetch from the JSON configs embedded
//  as RCDATA resources in Casso.exe. These tests load Casso.exe as
//  a resource-only module, extract each machine's embedded JSON,
//  and assert exactly which ROM files MachineConfigLoader sees.
//
//  No filesystem access to user data — only loading our own build
//  artifact (Casso.exe) as a resource module, which is part of the
//  solution under test, not "system state".
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AssetBootstrapTests)
{
public:

    TEST_METHOD (Embedded_AppleII_RequiresSystemCharacterAndDisk2Rom)
    {
        std::vector<std::string> files;

        AssertRomList (IDR_MACHINE_APPLE2, files);

        Assert::AreEqual (size_t (3), files.size(),
            L"Apple2 must reference exactly 3 ROMs "
            L"(system + character + Disk II slot)");
        Assert::AreEqual (std::string ("Apple2.rom"),       files[0],
            L"Apple2 system ROM must be Apple2.rom");
        Assert::AreEqual (std::string ("Apple2_Video.rom"), files[1],
            L"Apple2 character ROM must be Apple2_Video.rom");
        Assert::AreEqual (std::string ("Disk2.rom"),        files[2],
            L"Apple2 slot 6 ROM must be Disk2.rom");
    }

    TEST_METHOD (Embedded_AppleIIPlus_RequiresSystemCharacterAndDisk2Rom)
    {
        std::vector<std::string> files;

        AssertRomList (IDR_MACHINE_APPLE2PLUS, files);

        Assert::AreEqual (size_t (3), files.size(),
            L"Apple2Plus must reference exactly 3 ROMs "
            L"(system + character + Disk II slot)");
        Assert::AreEqual (std::string ("Apple2Plus.rom"),   files[0],
            L"Apple2Plus system ROM must be Apple2Plus.rom");
        Assert::AreEqual (std::string ("Apple2_Video.rom"), files[1],
            L"Apple2Plus character ROM must be Apple2_Video.rom");
        Assert::AreEqual (std::string ("Disk2.rom"),        files[2],
            L"Apple2Plus slot 6 ROM must be Disk2.rom");
    }

    TEST_METHOD (Embedded_Apple2e_RequiresSystemCharacterAndDisk2Rom)
    {
        std::vector<std::string> files;

        AssertRomList (IDR_MACHINE_APPLE2E, files);

        Assert::AreEqual (size_t (3), files.size(),
            L"Apple2e must reference exactly 3 ROMs "
            L"(system + character + Disk II slot)");
        Assert::AreEqual (std::string ("Apple2e.rom"),       files[0],
            L"Apple2e system ROM must be Apple2e.rom");
        Assert::AreEqual (std::string ("Apple2e_Video.rom"), files[1],
            L"Apple2e character ROM must be Apple2e_Video.rom");
        Assert::AreEqual (std::string ("Disk2.rom"),         files[2],
            L"Apple2e slot 6 ROM must be Disk2.rom");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Embedded_*_DiskController
    //
    //  AssetBootstrap::HasDiskController inspects the embedded JSON
    //  for a slot whose `device == "disk-ii"` to decide whether to
    //  offer the user a boot-disk download. Lock down the per-machine
    //  result so a config edit can never silently drop the disk
    //  controller (and skip the boot-disk prompt) for a //e.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Embedded_AppleII_HasDiskController)
    {
        Assert::IsTrue (EmbeddedHasDiskController (IDR_MACHINE_APPLE2),
            L"Apple ][ ships with a slot 6 Disk ][ controller by default "
            L"so the user gets a working disk drive without manual config");
    }

    TEST_METHOD (Embedded_AppleIIPlus_HasDiskController)
    {
        Assert::IsTrue (EmbeddedHasDiskController (IDR_MACHINE_APPLE2PLUS),
            L"Apple ][+ ships with a slot 6 Disk ][ controller by default");
    }

    TEST_METHOD (Embedded_Apple2e_HasDiskController)
    {
        Assert::IsTrue (EmbeddedHasDiskController (IDR_MACHINE_APPLE2E),
            L"Apple //e must declare a slot 6 disk-ii device so the "
            L"first-run boot-disk prompt actually fires");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Embedded_StampMatchesEachJsonCassoMachineVersion
    //
    //  The invariant EnsureMachineConfigs' _DEBUG self-check guards at
    //  startup, lifted into CI: every s_kEmbeddedConfigs stamp must equal the
    //  $cassoMachineVersion baked into that machine's embedded JSON. A stamp
    //  that drifts below its JSON silently skips a real config upgrade -- the
    //  game-port commit once bumped Apple2.json 6 -> 7 but left the stamp at
    //  6, hiding the ][ game port from existing users. Reads the real stamps
    //  from the shared header so a copy can't rot out of sync.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Embedded_StampMatchesEachJsonCassoMachineVersion)
    {
        for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
        {
            std::string     jsonText = LoadEmbeddedJson (cfg.resourceId);
            std::wstring    machine  (cfg.machineName.begin(), cfg.machineName.end());
            JsonValue       root;
            JsonParseError  parseError;
            int             version  = 0;
            HRESULT         hrParse  = S_OK;
            HRESULT         hrVer    = S_OK;


            hrParse = JsonParser::Parse (jsonText, root, parseError);
            Assert::IsTrue (SUCCEEDED (hrParse),
                std::format (L"{} embedded JSON must parse", machine).c_str());

            hrVer = root.GetInt ("$cassoMachineVersion", version);
            Assert::IsTrue (SUCCEEDED (hrVer),
                std::format (L"{} embedded JSON must carry $cassoMachineVersion", machine).c_str());

            Assert::AreEqual (cfg.currentVersion, version,
                std::format (L"{} s_kEmbeddedConfigs stamp ({}) must equal its embedded "
                             L"$cassoMachineVersion ({})", machine, cfg.currentVersion, version).c_str());
        }
    }

private:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  AssertRomList
    //
    //  Loads Casso.exe as a resource-only module, extracts the
    //  embedded JSON for the given resource id, and runs
    //  MachineConfigLoader::CollectRomFiles on it. Populates
    //  `outFiles` and asserts both the load and the collect
    //  succeeded so callers can keep the per-test bodies tight.
    //
    ////////////////////////////////////////////////////////////////////////////

    void AssertRomList (int resourceId, std::vector<std::string> & outFiles)
    {
        std::string  jsonText = LoadEmbeddedJson (resourceId);
        std::string  error;
        HRESULT      hr       = S_OK;


        hr = MachineConfigLoader::CollectRomFiles (jsonText, outFiles, error);

        Assert::IsTrue (SUCCEEDED (hr),
            std::format (L"CollectRomFiles failed on embedded JSON: {}",
                         std::wstring (error.begin(), error.end())).c_str());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  EmbeddedHasDiskController
    //
    //  Mirrors AssetBootstrap::HasDiskController: scans the embedded
    //  JSON for a slot with `device == "disk-ii"`. Test-side copy so
    //  the test DLL doesn't have to link against AssetBootstrap.cpp
    //  (which lives in the Casso.exe project, not a static lib).
    //
    ////////////////////////////////////////////////////////////////////////////

    bool EmbeddedHasDiskController (int resourceId)
    {
        std::string         jsonText = LoadEmbeddedJson (resourceId);
        JsonValue           root;
        JsonParseError      parseError;
        const JsonValue   * pSlots   = nullptr;
        HRESULT             hrParse  = S_OK;
        HRESULT             hrSlots  = S_OK;
        bool                found    = false;
        std::string         device;


        hrParse = JsonParser::Parse (jsonText, root, parseError);
        Assert::IsTrue (SUCCEEDED (hrParse),
            L"Embedded JSON must parse cleanly");

        hrSlots = root.GetArray ("slots", pSlots);

        if (FAILED (hrSlots))
        {
            return false;
        }

        for (size_t idx = 0; idx < pSlots->ArraySize(); idx++)
        {
            const JsonValue &  entry = pSlots->ArrayAt (idx);
            HRESULT            hrDev = entry.GetString ("device", device);

            if (SUCCEEDED (hrDev) && device == "disk-ii")
            {
                found = true;
                break;
            }
        }

        return found;
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  LoadEmbeddedJson
    //
    //  Loads Casso.exe as a resource-only module and extracts the
    //  RCDATA bytes for `resourceId` as a string. Asserts on every
    //  failure point so callers can keep test bodies tight.
    //
    ////////////////////////////////////////////////////////////////////////////

    std::string LoadEmbeddedJson (int resourceId)
    {
        HMODULE          hExe       = nullptr;
        HRSRC            hRes       = nullptr;
        HGLOBAL          hMem       = nullptr;
        DWORD            size       = 0;
        const void     * data       = nullptr;
        std::string      jsonText;
        fs::path         exePath    = LocateCassoExe();


        if (exePath.empty())
        {
            Assert::Fail (L"Casso.exe not found next to the test DLL");
            return jsonText;
        }

        hExe = LoadLibraryExW (exePath.wstring().c_str(),
                               nullptr,
                               LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);

        if (hExe == nullptr)
        {
            Assert::Fail (std::format (L"LoadLibraryExW failed for {}",
                                       exePath.wstring()).c_str());
            return jsonText;
        }

        hRes = FindResourceW (hExe, MAKEINTRESOURCEW (resourceId), RT_RCDATA);

        if (hRes == nullptr)
        {
            FreeLibrary (hExe);
            Assert::Fail (L"Embedded RCDATA resource not found in Casso.exe");
            return jsonText;
        }

        size = SizeofResource (hExe, hRes);

        if (size == 0)
        {
            FreeLibrary (hExe);
            Assert::Fail (L"Embedded resource is empty");
            return jsonText;
        }

        hMem = LoadResource (hExe, hRes);

        if (hMem == nullptr)
        {
            FreeLibrary (hExe);
            Assert::Fail (L"LoadResource failed");
            return jsonText;
        }

        data = LockResource (hMem);

        if (data == nullptr)
        {
            FreeLibrary (hExe);
            Assert::Fail (L"LockResource failed");
            return jsonText;
        }

        jsonText.assign (static_cast<const char *> (data), size);

        FreeLibrary (hExe);

        return jsonText;
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  LocateCassoExe
    //
    //  Find Casso.exe by walking out from the test DLL's own module
    //  directory (vstest places both binaries in the same output
    //  folder). Returns an empty path if not found.
    //
    ////////////////////////////////////////////////////////////////////////////

    static fs::path LocateCassoExe()
    {
        wchar_t   buf[MAX_PATH] = {};
        HMODULE   hSelf         = nullptr;
        BOOL      ok            = FALSE;
        fs::path  candidate;


        ok = GetModuleHandleExW (
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR> (&LocateCassoExe),
            &hSelf);

        if (!ok || hSelf == nullptr)
        {
            return {};
        }

        if (GetModuleFileNameW (hSelf, buf, MAX_PATH) == 0)
        {
            return {};
        }

        candidate = fs::path (buf).parent_path() / L"Casso.exe";

        if (!fs::exists (candidate))
        {
            return {};
        }

        return candidate;
    }
};
