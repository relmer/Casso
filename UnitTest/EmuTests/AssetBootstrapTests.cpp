#include "Pch.h"
#include "Core/JsonParser.h"
#include "Machines/MachineDefinitions.h"
#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"
#include "resource.h"
#include "EmbeddedMachineConfigs.h"
#include "EmbeddedMachineJson.h"

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

    TEST_METHOD (Embedded_Apple2eEnhanced_RequiresSystemCharacterAndDisk2Rom)
    {
        std::vector<std::string> files;

        AssertRomList (IDR_MACHINE_APPLE2E_ENHANCED, files);

        Assert::AreEqual (size_t (3), files.size(),
            L"Apple2eEnhanced must reference exactly 3 ROMs "
            L"(system + character + Disk II slot)");
        Assert::AreEqual (std::string ("Apple2eEnhanced.rom"), files[0],
            L"Apple2eEnhanced system ROM must be the enhanced //e ROM");
        Assert::AreEqual (std::string ("Apple2e_Video.rom"),   files[1],
            L"Apple2eEnhanced shares the //e MouseText character ROM");
        Assert::AreEqual (std::string ("Disk2.rom"),           files[2],
            L"Apple2eEnhanced slot 6 ROM must be Disk2.rom");
    }

    // The Enhanced //e is defined by its 65C02: the enhanced ROM runs CMOS
    // opcodes the NMOS //e cannot, which is the whole reason the profile
    // exists. Lock the CPU string so the embed can never
    // silently ship a 6502 that would crash on the enhanced firmware.
    TEST_METHOD (Embedded_Apple2eEnhanced_UsesCmos65C02)
    {
        //
        //  The CPU is no longer in the document. Which processor a shipped
        //  machine has is not something its owner configures, so it is stated
        //  in code and the file does not repeat it. The assertion follows it
        //  there rather than being dropped -- the //e Enhanced running a 65C02
        //  is the whole of what made it enhanced.
        //
        const MachineDefinition *  definition = MachineDefinitions::Find ("Apple2eEnhanced");

        Assert::IsNotNull (definition,
            L"Apple2eEnhanced ships, so it has a definition");
        Assert::AreEqual (std::string ("65C02"), definition->cpu,
            L"Apple2eEnhanced must select the 65C02 core");
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

    TEST_METHOD (Embedded_Apple2eEnhanced_HasDiskController)
    {
        Assert::IsTrue (EmbeddedHasDiskController (IDR_MACHINE_APPLE2E_ENHANCED),
            L"Apple //e Enhanced must declare a slot 6 disk-ii device so the "
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
            std::string     jsonText = EmbeddedMachineJson::Load (cfg.resourceId);
            JsonValue       root;
            JsonParseError  parseError;
            int             version  = 0;
            HRESULT         hrParse  = S_OK;
            HRESULT         hrVer    = S_OK;
            std::wstring    machine  (cfg.machineName.begin(), cfg.machineName.end());


            hrParse = JsonParser::Parse (jsonText, root, parseError);
            AssertSucceeded (hrParse,
                std::format (L"{} embedded JSON must parse", machine).c_str());

            hrVer = root.GetInt ("$cassoMachineVersion", version);
            AssertSucceeded (hrVer,
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
        std::string  jsonText = EmbeddedMachineJson::Load (resourceId);
        std::string  error;
        HRESULT      hr       = S_OK;


        hr = MachineConfigLoader::CollectRomFiles (jsonText, outFiles, error);

        AssertSucceeded (hr,
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
        std::string         jsonText = EmbeddedMachineJson::Load (resourceId);
        JsonValue           root;
        JsonParseError      parseError;
        const JsonValue   * pSlots   = nullptr;
        HRESULT             hrParse  = S_OK;
        HRESULT             hrSlots  = S_OK;
        bool                found    = false;
        std::string         device;


        hrParse = JsonParser::Parse (jsonText, root, parseError);
        AssertSucceeded (hrParse,
            L"Embedded JSON must parse cleanly");

        hrSlots = root.GetArray ("slots", pSlots);

        // A config with no "slots" array simply has no cards in it -- that is
        // a valid machine (the //c has its drives on-board), not a parse error.
        if (SUCCEEDED (hrSlots))
        {
            for (size_t idx = 0; !found && idx < pSlots->GetArraySize(); idx++)
            {
                const JsonValue &  entry = pSlots->GetArrayElement (idx);
                HRESULT            hrDev = entry.GetString ("device", device);

                found = (SUCCEEDED (hrDev) && device == "disk-ii");
            }
        }

        return found;
    }
};
