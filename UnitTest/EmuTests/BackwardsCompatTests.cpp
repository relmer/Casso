#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "Core/MachineConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr int       kMaxAncestorWalk     = 8;
    static constexpr size_t    kRom12K              = 12288;
    static constexpr size_t    kRom16K              = 16384;
    static constexpr size_t    kCharRomSize         = 2048;
    static constexpr size_t    kDiskRomSize         = 256;
    static constexpr size_t    kEnhancedCharRomSize = 4096;
    static constexpr size_t    kPrngSampleCount     = 256;
    static constexpr size_t    kAppleIIVideoModes   = 3;
    static constexpr Word      kAppleIISystemRomAt  = 0xD000;
    static constexpr Word      kAppleIIRamSize      = 0xC000;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  WalkUpForRepoRoot — locate the directory containing `Resources/` by
    //  walking up the test binary's working directory. Mirrors the
    //  resolver pattern used by FixtureProvider so the tests stay
    //  filesystem-independent across CI vs local builds. Resources/
    //  is always tracked in git (unlike runtime-managed Machines/).
    //
    ////////////////////////////////////////////////////////////////////////////

    fs::path WalkUpForRepoRoot ()
    {
        std::error_code   ec;
        fs::path          cursor;
        fs::path          candidate;
        int               steps;

        cursor = fs::current_path (ec);
        if (ec)
        {
            return fs::path ();
        }

        for (steps = 0; steps < kMaxAncestorWalk; steps++)
        {
            candidate = cursor / "Resources";

            if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
            {
                return cursor;
            }

            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }

            cursor = cursor.parent_path ();
        }

        return fs::path ();
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ReadMachineJson — read a Resources/Machines/<MachineName>/<MachineName>.json
    //  file from the resolved repo root into a string. Returns "" if
    //  not found. Accepts the same `<MachineName>.json` filename the
    //  callers used under the legacy flat layout; the per-machine
    //  subdirectory is derived by stripping the `.json` suffix.
    //
    ////////////////////////////////////////////////////////////////////////////

    std::string ReadMachineJson (const std::string & filename)
    {
        fs::path        repoRoot = WalkUpForRepoRoot ();
        fs::path        full;
        std::string     stem;
        std::ifstream   stream;
        std::string     content;

        if (repoRoot.empty ())
        {
            return std::string ();
        }

        stem = fs::path (filename).stem ().string ();
        full = repoRoot / "Resources" / "Machines" / stem / filename;

        stream.open (full, std::ios::binary);
        if (!stream.is_open ())
        {
            return std::string ();
        }

        content.assign (
            std::istreambuf_iterator<char> (stream),
            std::istreambuf_iterator<char> ());

        return content;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MockResolveAll — stamps a temp file of the expected size for each
    //  ROM filename so MachineConfigLoader::Load passes its size-check.
    //  Mirrors MachineConfigTests::MockResolveAll so backwards-compat
    //  parsing exercises the same code path that ships in production.
    //
    ////////////////////////////////////////////////////////////////////////////

    fs::path MockResolveAll (
        const std::vector<fs::path> & searchPaths,
        const fs::path              & relativePath)
    {
        std::string         filename = relativePath.filename ().string ();
        size_t              expectedSize;
        fs::path            tempPath;
        bool                needCreate;
        std::error_code     ec;
        std::vector<Byte>   buffer;
        std::ofstream       out;

        UNREFERENCED_PARAMETER (searchPaths);

        expectedSize = kDiskRomSize;

        if (filename == "Apple2Plus.rom" || filename == "Apple2.rom")
        {
            expectedSize = kRom12K;
        }
        else if (filename == "Apple2e.rom" || filename == "Apple2eEnhanced.rom")
        {
            expectedSize = kRom16K;
        }
        else if (filename == "Disk2.rom")
        {
            expectedSize = kDiskRomSize;
        }
        else if (filename == "Apple2_Video.rom")
        {
            expectedSize = kCharRomSize;
        }
        else if (filename == "Apple2e_Video.rom")
        {
            expectedSize = kEnhancedCharRomSize;
        }

        tempPath   = fs::temp_directory_path () / ("casso_bc_" + filename);
        needCreate = !fs::exists (tempPath, ec);

        if (!needCreate)
        {
            try
            {
                needCreate = fs::file_size (tempPath) != expectedSize;
            }
            catch (...)
            {
                needCreate = true;
            }
        }

        if (needCreate)
        {
            buffer.assign (expectedSize, 0);
            out.open (tempPath, std::ios::binary | std::ios::trunc);
            out.write (reinterpret_cast<const char *> (buffer.data ()),
                       static_cast<std::streamsize> (expectedSize));
            out.flush ();
            out.close ();
        }

        return tempPath;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  HasInternalDeviceType — true if `cfg` lists a device whose `type`
    //  matches `needle`. Used to assert the //e-only device types are
    //  absent from the ][/][+ configs.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool HasInternalDeviceType (const MachineConfig & cfg, const std::string & needle)
    {
        size_t   i;

        for (i = 0; i < cfg.internalDevices.size (); i++)
        {
            if (cfg.internalDevices[i].type == needle)
            {
                return true;
            }
        }

        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackwardsCompatTests — Phase 14 / User Story 5.
//
//  Consolidates the explicit ][/][+ regression gate. The whole point of
//  the //e fidelity feature is that it MUST NOT regress original Apple
//  ][ or Apple ][+ behavior (FR-039) and MUST be implemented through
//  composition rather than branching the existing ][/][+ machine
//  configurations or the existing devices (FR-040).
//
//  Two distinct surfaces are pinned here:
//
//    1. JSON pin — `Machines/Apple2.json` and `Machines/Apple2Plus.json`
//       remain byte-identical to their pre-feature shape; their device
//       lists contain ONLY the original ][/][+ device types and explicitly
//       NONE of the //e-specific types (apple2e-keyboard,
//       apple2e-softswitches, apple2e-mmu, language-card). Their RAM map
//       is single-bank ($0000-$BFFF) with no `aux` bank. Their video-mode
//       list contains exactly text40/lores/hires (no text80/dhgr).
//
//    2. Composition pin — HeadlessHost::BuildAppleII /
//       BuildAppleIIPlus continue to compose only the deterministic
//       harness primitives (Prng, MockHostShell, FixtureProvider) and
//       MUST NOT pull in the //e wiring (no AppleIIeMmu, no EmuCpu,
//       no aux RAM, no LanguageCardBank). This is the architectural
//       proof that the //e build path is a *separate* composition, not
//       a branch of the ][/][+ build path.
//
//  Together, these tests serve as the "must never change byte-exactly"
//  baseline. If any assertion in this file would have to change for a
//  later-phase change to compile or pass, that's a regression — the fix
//  belongs in the production code, never in this file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BackwardsCompatTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_Json_ParsesAsValidMachineConfig — the original ][ config
    //  still loads through the same MachineConfigLoader path used in
    //  production.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_Json_ParsesAsValidMachineConfig)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2.json");
        Assert::IsFalse (json.empty (),
            L"Resources/Machines/Apple2/Apple2.json must be reachable from the test cwd");

        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            L"Apple2.json must parse cleanly through the production loader");
        Assert::AreEqual (std::string ("Apple ]["), config.name,
            L"Apple2.json name must remain 'Apple ]['");
        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"Apple2.json CPU must remain '6502'");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_Json_ParsesAsValidMachineConfig — same gate for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_Json_ParsesAsValidMachineConfig)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2Plus.json");
        Assert::IsFalse (json.empty (),
            L"Resources/Machines/Apple2Plus/Apple2Plus.json must be reachable from the test cwd");

        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            L"Apple2Plus.json must parse cleanly through the production loader");
        Assert::AreEqual (std::string ("Apple ][ plus"), config.name,
            L"Apple2Plus.json name must remain 'Apple ][ plus'");
        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"Apple2Plus.json CPU must remain '6502'");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_NoMmuPresent — the ][ config's internalDevices list MUST
    //  NOT contain `apple2e-mmu`. This is the FR-040 composition pin:
    //  the //e MMU is a //e-only device, never bolted onto ][.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_NoMmuPresent)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-mmu"),
            L"Apple2.json must NOT include apple2e-mmu (composition pin)");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-keyboard"),
            L"Apple2.json must NOT include apple2e-keyboard");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-softswitches"),
            L"Apple2.json must NOT include apple2e-softswitches");
        Assert::IsFalse (HasInternalDeviceType (config, "language-card"),
            L"Apple2.json must NOT include language-card (//e-only here)");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_NoMmuPresent — same pin for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_NoMmuPresent)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-mmu"),
            L"Apple2Plus.json must NOT include apple2e-mmu");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-keyboard"),
            L"Apple2Plus.json must NOT include apple2e-keyboard");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-softswitches"),
            L"Apple2Plus.json must NOT include apple2e-softswitches");
        Assert::IsFalse (HasInternalDeviceType (config, "language-card"),
            L"Apple2Plus.json must NOT include language-card");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_NoAuxRam_NoExtendedVideoModes — the ][ has no aux RAM
    //  bank and only the original three video modes (text40, lores,
    //  hires). 80-column / double-hires modes are //e-only and MUST
    //  remain absent from the ][ config.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_NoAuxRam_NoExtendedVideoModes)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        size_t                  i;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (size_t (1), config.ram.size (),
            L"Apple2.json must declare exactly one RAM region (no aux bank)");
        Assert::AreEqual (Word (0x0000), config.ram[0].address,
            L"Apple2.json RAM region 0 must start at $0000");
        Assert::AreEqual (kAppleIIRamSize, config.ram[0].size,
            L"Apple2.json RAM region 0 must be $C000 bytes");
        Assert::AreEqual (kAppleIISystemRomAt, config.systemRom.address,
            L"Apple2.json system ROM must remain at $D000");

        Assert::AreEqual (kAppleIIVideoModes, config.videoConfig.modes.size (),
            L"Apple2.json must list exactly 3 video modes (text40/lores/hires)");

        for (i = 0; i < config.videoConfig.modes.size (); i++)
        {
            Assert::AreNotEqual (std::string ("apple2-text80"),
                config.videoConfig.modes[i],
                L"Apple2.json must NOT include 80-col text mode");
            Assert::AreNotEqual (std::string ("apple2-doublehires"),
                config.videoConfig.modes[i],
                L"Apple2.json must NOT include double-hires mode");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_NoAuxRam_NoExtendedVideoModes — same surface for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_NoAuxRam_NoExtendedVideoModes)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        size_t                  i;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (size_t (1), config.ram.size (),
            L"Apple2Plus.json must declare exactly one RAM region");
        Assert::AreEqual (kAppleIIRamSize, config.ram[0].size,
            L"Apple2Plus.json RAM region must be $C000 bytes");
        Assert::AreEqual (kAppleIISystemRomAt, config.systemRom.address,
            L"Apple2Plus.json system ROM must remain at $D000");
        Assert::AreEqual (kAppleIIVideoModes, config.videoConfig.modes.size (),
            L"Apple2Plus.json must list exactly 3 video modes");

        for (i = 0; i < config.videoConfig.modes.size (); i++)
        {
            Assert::AreNotEqual (std::string ("apple2-text80"),
                config.videoConfig.modes[i],
                L"Apple2Plus.json must NOT include 80-col text mode");
            Assert::AreNotEqual (std::string ("apple2-doublehires"),
                config.videoConfig.modes[i],
                L"Apple2Plus.json must NOT include double-hires mode");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_KeyboardAndDevices_Unchanged — the ][ keyboard remains
    //  `apple2-uppercase` (NOT `apple2e-full`) and the device set is
    //  exactly { apple2-keyboard, apple2-speaker, apple2-softswitches }.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_KeyboardAndDevices_Unchanged)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (std::string ("apple2-uppercase"), config.keyboardType,
            L"Apple2.json keyboard type must remain apple2-uppercase");

        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-keyboard"),
            L"Apple2.json must keep apple2-keyboard");
        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-speaker"),
            L"Apple2.json must keep apple2-speaker");
        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-softswitches"),
            L"Apple2.json must keep apple2-softswitches");

        Assert::AreEqual (size_t (3), config.internalDevices.size (),
            L"Apple2.json internalDevices count must remain exactly 3");
        Assert::AreEqual (size_t (0), config.slots.size (),
            L"Apple2.json must declare zero slot devices by default");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_KeyboardAndDevices_Unchanged — same gate for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_KeyboardAndDevices_Unchanged)
    {
        std::string             json;
        MachineConfig           config;
        std::string             error;
        std::vector<fs::path>   searchPaths;
        HRESULT                 hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "TestMachine", searchPaths, MockResolveAll,
                                        config, error);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (std::string ("apple2-uppercase"), config.keyboardType,
            L"Apple2Plus.json keyboard type must remain apple2-uppercase");

        Assert::IsTrue (HasInternalDeviceType (config, "apple2-keyboard"),
            L"Apple2Plus.json must keep apple2-keyboard");
        Assert::IsTrue (HasInternalDeviceType (config, "apple2-speaker"),
            L"Apple2Plus.json must keep apple2-speaker");
        Assert::IsTrue (HasInternalDeviceType (config, "apple2-softswitches"),
            L"Apple2Plus.json must keep apple2-softswitches");

        Assert::AreEqual (size_t (3), config.internalDevices.size (),
            L"Apple2Plus.json internalDevices count must remain exactly 3");
        Assert::AreEqual (size_t (0), config.slots.size (),
            L"Apple2Plus.json must declare zero slot devices by default");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_HeadlessHost_Composes — BuildAppleII succeeds and produces
    //  a deterministic harness with NO //e wiring attached. The whole
    //  point of the FR-040 composition pin: the //e build path lives in
    //  a separate function (BuildAppleIIe) that adds CPU + MMU + bus on
    //  top; the ][ build path is intentionally minimal.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_HeadlessHost_Composes)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        HRESULT        hr;

        hr = host.BuildAppleII (core);

        Assert::IsTrue (SUCCEEDED (hr),
            L"HeadlessHost::BuildAppleII must succeed");
        Assert::IsTrue (core.machineKind == HeadlessMachineKind::AppleII,
            L"machineKind must remain AppleII");

        Assert::IsNotNull (core.prng.get (),     L"][ harness must wire a Prng");
        Assert::IsNotNull (core.host.get (),     L"][ harness must wire MockHostShell");
        Assert::IsNotNull (core.fixtures.get (), L"][ harness must wire FixtureProvider");

        Assert::IsNull (core.mmu.get (),
            L"][ harness must NOT pull in AppleIIeMmu (composition pin)");
        Assert::IsNull (core.cpu.get (),
            L"][ harness must NOT pull in EmuCpu (][ build path stays minimal)");
        Assert::IsNull (core.bus.get (),
            L"][ harness must NOT pull in MemoryBus");
        Assert::IsNull (core.mainRam.get (),
            L"][ harness must NOT pull in RamDevice");
        Assert::IsNull (core.languageCard.get (),
            L"][ harness must NOT pull in LanguageCard by default");
        Assert::IsNull (core.diskController.get (),
            L"][ harness must NOT pull in DiskIIController by default");

        Assert::IsFalse (core.HasAppleIIe (),
            L"][ harness must NOT report HasAppleIIe");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_HeadlessHost_Composes — same pin for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_HeadlessHost_Composes)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        HRESULT        hr;

        hr = host.BuildAppleIIPlus (core);

        Assert::IsTrue (SUCCEEDED (hr),
            L"HeadlessHost::BuildAppleIIPlus must succeed");
        Assert::IsTrue (core.machineKind == HeadlessMachineKind::AppleIIPlus,
            L"machineKind must remain AppleIIPlus");

        Assert::IsNotNull (core.prng.get ());
        Assert::IsNotNull (core.host.get ());
        Assert::IsNotNull (core.fixtures.get ());

        Assert::IsNull (core.mmu.get (),
            L"][+ harness must NOT pull in AppleIIeMmu");
        Assert::IsNull (core.cpu.get (),
            L"][+ harness must NOT pull in EmuCpu");
        Assert::IsNull (core.bus.get ());
        Assert::IsNull (core.mainRam.get ());
        Assert::IsNull (core.languageCard.get ());
        Assert::IsNull (core.diskController.get ());

        Assert::IsFalse (core.HasAppleIIe ());
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_DeterministicAcrossTwoBuilds — the ][ harness's pinned
    //  Prng seed produces byte-identical output across two independent
    //  builds. Same gate that HeadlessHostTests applies for //e — the
    //  point here is that the deterministic guarantee extends to the
    //  ][/][+ build path too (no machine-kind-specific seed drift).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_DeterministicAcrossTwoBuilds)
    {
        HeadlessHost   hostA;
        HeadlessHost   hostB;
        EmulatorCore   coreA;
        EmulatorCore   coreB;
        size_t         i;
        HRESULT        hr;

        hr = hostA.BuildAppleII (coreA);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = hostB.BuildAppleII (coreB);
        Assert::IsTrue (SUCCEEDED (hr));

        for (i = 0; i < kPrngSampleCount; i++)
        {
            Assert::AreEqual (coreA.prng->Next64 (), coreB.prng->Next64 (),
                L"][ harness with the pinned seed must be deterministic");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_DeterministicAcrossTwoBuilds — same gate for ][+.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_DeterministicAcrossTwoBuilds)
    {
        HeadlessHost   hostA;
        HeadlessHost   hostB;
        EmulatorCore   coreA;
        EmulatorCore   coreB;
        size_t         i;
        HRESULT        hr;

        hr = hostA.BuildAppleIIPlus (coreA);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = hostB.BuildAppleIIPlus (coreB);
        Assert::IsTrue (SUCCEEDED (hr));

        for (i = 0; i < kPrngSampleCount; i++)
        {
            Assert::AreEqual (coreA.prng->Next64 (), coreB.prng->Next64 (),
                L"][+ harness with the pinned seed must be deterministic");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  MachineKinds_RemainDistinct — the three HeadlessMachineKind enum
    //  values exist as separate identities so the build paths stay
    //  composable. If anyone ever collapses ][ into //e via
    //  branching, this test breaks immediately.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (MachineKinds_RemainDistinct)
    {
        HeadlessHost   host;
        EmulatorCore   coreII;
        EmulatorCore   coreIIPlus;
        EmulatorCore   coreIIe;
        HRESULT        hr;

        hr = host.BuildAppleII (coreII);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = host.BuildAppleIIPlus (coreIIPlus);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = host.BuildAppleIIe (coreIIe);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsTrue (coreII.machineKind     == HeadlessMachineKind::AppleII);
        Assert::IsTrue (coreIIPlus.machineKind == HeadlessMachineKind::AppleIIPlus);
        Assert::IsTrue (coreIIe.machineKind    == HeadlessMachineKind::AppleIIe);

        Assert::IsTrue (coreII.machineKind     != coreIIPlus.machineKind);
        Assert::IsTrue (coreIIPlus.machineKind != coreIIe.machineKind);
        Assert::IsTrue (coreII.machineKind     != coreIIe.machineKind);

        Assert::IsTrue  (coreIIe.HasAppleIIe (),
            L"//e build path must produce a fully wired //e core");
        Assert::IsFalse (coreII.HasAppleIIe (),
            L"][ build path must NOT produce a //e core");
        Assert::IsFalse (coreIIPlus.HasAppleIIe (),
            L"][+ build path must NOT produce a //e core");
    }
};
