#include "Pch.h"
#include "TestMachine.h"
#include "Core/MachineConfig.h"
#include "Machines/MachineDefinitions.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





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
//    2. Composition pin — the ][ and ][+ build from
//       BuildAppleIIPlus continue to compose only the deterministic
//       harness primitives (Prng, FixtureProvider) and
//       MUST NOT pull in the //e wiring (no Apple2eMmu, no EmuCpu,
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

    fs::path WalkUpForRepoRoot()
    {
        std::error_code   ec;
        fs::path          cursor;
        fs::path          candidate;
        int               steps;

        fs::path   root;
        bool       walking;

        cursor  = fs::current_path (ec);
        walking = !ec;

        for (steps = 0; walking && root.empty() && steps < kMaxAncestorWalk; steps++)
        {
            candidate = cursor / "Resources";

            if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
            {
                root = cursor;
            }
            else if (!cursor.has_parent_path() || cursor == cursor.parent_path())
            {
                // Hit the drive root without finding Resources/.
                walking = false;
            }
            else
            {
                cursor = cursor.parent_path();
            }
        }

        return root;
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
        fs::path        repoRoot = WalkUpForRepoRoot();
        fs::path        full;
        std::string     stem;
        std::ifstream   stream;
        std::string     content;

        // No repo root means no Resources/ tree to read from -- there is
        // nothing to open, so this stops before building a path from an
        // empty prefix (which would resolve relative to the CWD).
        if (!repoRoot.empty())
        {
            stem = fs::path (filename).stem().string();
            full = repoRoot / "Resources" / "Machines" / stem / filename;

            stream.open (full, std::ios::binary);

            if (stream.is_open())
            {
                content.assign (
                    std::istreambuf_iterator<char> (stream),
                    std::istreambuf_iterator<char> ());
            }
        }

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

    static fs::path MockResolveAll (
        const std::vector<fs::path> & searchPaths,
        const fs::path              & relativePath)
    {
        std::string         filename = relativePath.filename().string();
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
        else if (filename == "Apple2e_Video.rom" || filename == "Apple2eEnhanced_Video.rom")
        {
            expectedSize = kEnhancedCharRomSize;
        }

        tempPath   = fs::temp_directory_path() / ("casso_bc_" + filename);
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
            out.write (reinterpret_cast<const char *> (buffer.data()),
                       static_cast<std::streamsize> (expectedSize));
            out.flush();
            out.close();
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
        bool  found = false;



        for (auto it = cfg.internalDevices.begin();
             !found && it != cfg.internalDevices.end();
             ++it)
        {
            found = (it->type == needle);
        }

        return found;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  HasSlotDevice — true if `cfg` installs `device` in `slot`.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool HasSlotDevice (const MachineConfig & cfg, int slot, const std::string & device)
    {
        bool  found = false;



        for (auto it = cfg.slots.begin(); !found && it != cfg.slots.end(); ++it)
        {
            found = (it->slot == slot && it->device == device);
        }

        return found;
    }

    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleII_Json_ParsesAsValidMachineConfig — the original ][ config
    //  still loads through the same MachineConfigLoader path used in
    //  production.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleII_Json_ParsesAsValidMachineConfig)
    {
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2.json");
        Assert::IsFalse (json.empty(),
            L"Resources/Machines/Apple2/Apple2.json must be reachable from the test cwd");

        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2", searchPaths, MockResolveAll,
                                        config, error);

        AssertSucceeded (hr,
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
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2Plus.json");
        Assert::IsFalse (json.empty(),
            L"Resources/Machines/Apple2Plus/Apple2Plus.json must be reachable from the test cwd");

        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2Plus", searchPaths, MockResolveAll,
                                        config, error);

        AssertSucceeded (hr,
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
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-mmu"),
            L"Apple2.json must NOT include apple2e-mmu (composition pin)");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-keyboard"),
            L"Apple2.json must NOT include apple2e-keyboard");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-softswitches"),
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
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2Plus", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-mmu"),
            L"Apple2Plus.json must NOT include apple2e-mmu");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-keyboard"),
            L"Apple2Plus.json must NOT include apple2e-keyboard");
        Assert::IsFalse (HasInternalDeviceType (config, "apple2e-family-softswitches"),
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
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::AreEqual (size_t (1), config.ram.size(),
            L"Apple2.json must declare exactly one RAM region (no aux bank)");
        Assert::AreEqual (Word (0x0000), config.ram[0].address,
            L"Apple2.json RAM region 0 must start at $0000");
        Assert::AreEqual (kAppleIIRamSize, config.ram[0].size,
            L"Apple2.json RAM region 0 must be $C000 bytes");
        Assert::AreEqual (kAppleIISystemRomAt, config.systemRom.address,
            L"Apple2.json system ROM must remain at $D000");

        //  The mode list is the MACHINE's, not the document's: which video
        //  modes exist follows from whether the machine has an auxiliary bank
        //  to interleave from, so it is answered in code.
        modes = MachineDefinitions::FindMachine ("Apple2")->GetVideoModes();

        Assert::AreEqual (kAppleIIVideoModes, modes.size(),
            L"an Apple ][ offers exactly 3 video modes (text40/lores/hires)");

        for (auto & mode : modes)
        {
            Assert::AreNotEqual (std::string ("apple2-text80"),
                mode,
                L"Apple2.json must NOT include 80-col text mode");
            Assert::AreNotEqual (std::string ("apple2-doublehires"),
                mode,
                L"Apple2.json must NOT include double-hires mode");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_NoAuxRam_NoExtendedVideoModes
    //
    //  A ][+ config must declare NO aux RAM and none of the //e video modes.
    //
    //  An ABSENCE test, which is the shape backwards compatibility needs. //e
    //  support was added on top of the earlier machines, and the risk is not
    //  that the //e stops working -- it is that the ][+ quietly acquires
    //  capabilities it never had, so software that probes for them takes the
    //  wrong path.
    //
    //  It asserts against the shipped Apple2Plus.json rather than a fixture, so
    //  it fails if the config itself grows an aux region -- which is exactly
    //  the edit this guards against.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_NoAuxRam_NoExtendedVideoModes)
    {
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2Plus", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::AreEqual (size_t (1), config.ram.size(),
            L"Apple2Plus.json must declare exactly one RAM region");
        Assert::AreEqual (kAppleIIRamSize, config.ram[0].size,
            L"Apple2Plus.json RAM region must be $C000 bytes");
        Assert::AreEqual (kAppleIISystemRomAt, config.systemRom.address,
            L"Apple2Plus.json system ROM must remain at $D000");
        //  The mode list is the MACHINE's, not the document's: which video
        //  modes exist follows from whether the machine has an auxiliary bank
        //  to interleave from, so it is answered in code.
        modes = MachineDefinitions::FindMachine ("Apple2Plus")->GetVideoModes();

        Assert::AreEqual (kAppleIIVideoModes, modes.size(),
            L"an Apple ][+ offers exactly 3 video modes");

        for (auto & mode : modes)
        {
            Assert::AreNotEqual (std::string ("apple2-text80"),
                mode,
                L"Apple2Plus.json must NOT include 80-col text mode");
            Assert::AreNotEqual (std::string ("apple2-doublehires"),
                mode,
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
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::AreEqual (std::string ("apple2-family-layout"), config.keyboardType,
            L"Apple2.json keyboard type must remain apple2-uppercase");

        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-family-keyboard"),
            L"Apple2.json must keep apple2-keyboard");
        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-family-speaker"),
            L"Apple2.json must keep apple2-speaker");
        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-family-softswitches"),
            L"Apple2.json must keep apple2-softswitches");
        Assert::IsTrue  (HasInternalDeviceType (config, "apple2-family-gameport"),
            L"Apple2.json must include apple2-gameport");

        Assert::AreEqual (size_t (4), config.internalDevices.size(),
            L"Apple2.json internalDevices count must remain exactly 4");
        Assert::AreEqual (size_t (2), config.slots.size(),
            L"Apple2.json declares two slots: parallel printer (slot 1), Disk II (slot 6)");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  AppleIIPlus_KeyboardAndDevices_Unchanged
    //
    //  The ][+'s keyboard type and device list must stay exactly what they
    //  were.
    //
    //  A PIN, not a behavior test. The //e keyboard added modifier reporting
    //  and its soft-switch bank added switches the ][+ never had, and both are
    //  selected by config -- so this fails if the earlier machine is
    //  accidentally pointed at the later machine's parts.
    //
    //  Device identity is asserted by TYPE STRING rather than by count, since a
    //  substitution that keeps the count is precisely the change that would
    //  otherwise pass.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AppleIIPlus_KeyboardAndDevices_Unchanged)
    {
        std::string               json;
        MachineConfig             config;
        std::string               error;
        std::vector<fs::path>     searchPaths;
        std::vector<std::string>  modes;
        HRESULT                   hr;

        json = ReadMachineJson ("Apple2Plus.json");
        searchPaths.push_back (fs::path ("/mock"));

        hr = MachineConfigLoader::Load (json, "Apple2Plus", searchPaths, MockResolveAll,
                                        config, error);
        AssertSucceeded (hr);

        Assert::AreEqual (std::string ("apple2-family-layout"), config.keyboardType,
            L"Apple2Plus.json keyboard type must remain apple2-uppercase");

        Assert::IsTrue (HasInternalDeviceType (config, "apple2-family-keyboard"),
            L"Apple2Plus.json must keep apple2-keyboard");
        Assert::IsTrue (HasInternalDeviceType (config, "apple2-family-speaker"),
            L"Apple2Plus.json must keep apple2-speaker");
        Assert::IsTrue (HasInternalDeviceType (config, "apple2-family-softswitches"),
            L"Apple2Plus.json must keep apple2-softswitches");
        Assert::IsTrue (HasInternalDeviceType (config, "apple2-family-gameport"),
            L"Apple2Plus.json must include apple2-gameport");

        Assert::AreEqual (size_t (4), config.internalDevices.size(),
            L"Apple2Plus.json internalDevices count must remain exactly 4");
        Assert::AreEqual (size_t (3), config.slots.size(),
            L"Apple2Plus.json declares three slots: parallel printer (slot 1), Mockingboard (slot 4), Disk II (slot 6)");
        Assert::IsTrue (HasSlotDevice (config, 1, "parallel-printer"),
            L"Apple2Plus.json must install the parallel printer in slot 1");
        Assert::IsTrue (HasSlotDevice (config, 4, "mockingboard-c"),
            L"Apple2Plus.json must install the sound+speech Mockingboard in slot 4");
        Assert::IsTrue (HasSlotDevice (config, 6, "disk-ii"),
            L"Apple2Plus.json must keep the Disk II controller in slot 6");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  What the ][ and ][+ ARE, now that a test can build one.
    //
    //  These replace three tests that asserted the old harness had composed
    //  itself -- that it had wired a mock host, a fixture provider and a
    //  pinned Prng, and that its per-machine enum still held three distinct
    //  values. None of that was the machine. The harness built no ][ or ][+
    //  at all, so the strongest thing those tests could say about either
    //  model was that asking for one did not fail.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (TheAppleIIAndIIPlusAreDifferentMachines)
    {
        TestMachine  machineII     ("Apple2");
        TestMachine  machineIIPlus ("Apple2Plus");

        //  Integer BASIC and Applesoft: same size image, different ROM.
        std::vector<Byte>  romII     = GuestBytesAt (machineII,     0xE000, 64);
        std::vector<Byte>  romIIPlus = GuestBytesAt (machineIIPlus, 0xE000, 64);

        Assert::IsFalse (romII == romIIPlus,
            L"the ][ boots Integer BASIC and the ][+ Applesoft; their ROMs differ");
    }


    TEST_METHOD (NeitherHasTheIIeMemoryManager)
    {
        //  The composition pin, said about the machine rather than about
        //  which builder function ran: an MMU is //e-and-later hardware, and
        //  the earlier models must not acquire one.
        for (const char * id : { "Apple2", "Apple2Plus" })
        {
            TestMachine  machine (id);

            Assert::IsNull (machine.GetMmu(),
                L"a ][ or ][+ has no memory management unit");
            Assert::IsNull (machine.GetRefs().iieSoftSwitches,
                L"nor the //e soft-switch bank");
            Assert::IsNull (machine.GetRefs().iieKeyboard,
                L"nor the //e keyboard");
        }
    }


    TEST_METHOD (TwoBuildsOfOneMachineAgree)
    {
        TestMachine  a ("Apple2Plus");
        TestMachine  b ("Apple2Plus");

        a.PowerCycle();
        b.PowerCycle();

        //  The Prng is pinned, so the arbitrary contents a real machine
        //  powers on with are the same arbitrary contents twice running --
        //  which is what lets any test downstream assert on memory at all.
        Assert::IsTrue (GuestBytesAt (a, 0x2000, 256) == GuestBytesAt (b, 0x2000, 256),
            L"two builds of one machine power on identically");
    }


private:

    //  Straight off the bus, without running the guest.
    static std::vector<Byte> GuestBytesAt (MachineHost & machine, Word address, size_t count)
    {
        std::vector<Byte>  bytes;

        for (size_t i = 0; i < count; i++)
        {
            bytes.push_back (machine.GetMemoryBus().ReadByte (static_cast<Word> (address + i)));
        }

        return (bytes);
    }
};

