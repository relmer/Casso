#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"
#include "resource.h"

#include "EmbeddedMachineJson.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;


//  Long enough for a //e to clear its ROM's power-on self test and reach the
//  point where it is running from the monitor, and short enough to stay well
//  inside a unit test's budget: roughly a tenth of a second of guest time.
static constexpr uint32_t  s_kBootCycles = 100000;

static constexpr uint64_t  s_kSeed = 0xCA550031ULL;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuildTests
//
//  A real machine, built by the code that builds the real one.
//
//  The test tree used to carry its own //e, //c, ][ and ][+ builders -- 675
//  lines repeating the production wiring in the production order, because the
//  production builder was a method on a class that could not be constructed
//  without a window. Two copies of a wiring order is one copy too many: the
//  power-cycle sequences had already drifted apart, and the one under test
//  was the one nobody ships.
//
//  What makes this reachable is that every service the builder connects a
//  machine TO is optional. A machine with no mixer attached to its speaker
//  and no thread draining its printer is still the same machine, wired the
//  same way, by the same code.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineBuildTests)
{
public:

    TEST_METHOD (TheProductionBuilderBuildsAnAppleIIe)
    {
        MachineHost           host;
        MachineBuildServices  nothingListening;
        MachineBuilder        builder (host, nothingListening);
        MachineConfig         config;
        HRESULT               hr = S_OK;

        LoadConfig (IDR_MACHINE_APPLE2E, "Apple2e", config);

        host.SetPrng (std::make_unique<Prng> (s_kSeed));
        host.SetCurrentMachineName (L"Apple2e");
        host.GetConfig() = config;

        hr = builder.Build (config);
        AssertSucceeded (hr, L"the production builder must build a //e headlessly");

        //  The devices a //e has, found through the same cached pointers the
        //  renderer and the input path read.
        Assert::IsNotNull (host.GetCpu(),                     L"CPU");
        Assert::IsNotNull (host.GetMmu(),                     L"//e MMU");
        Assert::IsNotNull (host.GetRefs().keyboard,           L"keyboard");
        Assert::IsNotNull (host.GetRefs().softSwitches,       L"soft switches");
        Assert::IsNotNull (host.GetRefs().iieSoftSwitches,    L"//e soft switches");
        Assert::IsNotNull (host.GetRefs().speaker,            L"speaker");
        Assert::IsNotNull (host.GetRefs().mainRamDev,         L"main RAM");
        Assert::IsNotNull (host.GetRefs().diskController,     L"slot 6 Disk ][");
        Assert::IsNotNull (host.GetRefs().languageCard,       L"language card");

        //  All five renderers exist on every machine, because the per-frame
        //  mode selection switches between them and cannot afford to build
        //  one mid-render.
        Assert::IsNotNull (host.GetRefs().text40,      L"40-column text");
        Assert::IsNotNull (host.GetRefs().text80,      L"80-column text");
        Assert::IsNotNull (host.GetRefs().loRes,       L"lo-res");
        Assert::IsNotNull (host.GetRefs().hiRes,       L"hi-res");
        Assert::IsNotNull (host.GetRefs().doubleHiRes, L"double hi-res");
    }


    TEST_METHOD (ABuiltMachineBootsItsRom)
    {
        MachineHost           host;
        MachineBuildServices  nothingListening;
        MachineBuilder        builder (host, nothingListening);
        MachineConfig         config;
        uint64_t              spent = 0;

        Build (host, builder, IDR_MACHINE_APPLE2E, "Apple2e", config);

        host.PowerCycle();
        spent = host.RunCycles (s_kBootCycles);

        Assert::IsTrue (spent >= s_kBootCycles, L"the cycles asked for were spent");

        //  A //e that survived its self test is executing ROM. Falling into
        //  RAM, or sitting on the reset vector still, is what a machine that
        //  was mis-wired does -- and it is the only assertion here that
        //  depends on the wiring being RIGHT rather than merely present.
        Assert::IsTrue (host.GetCpu()->GetPC() >= 0xC000,
            std::format (L"a booted //e runs from ROM; PC was ${:04X}",
                         host.GetCpu()->GetPC()).c_str());
    }


    TEST_METHOD (TheSoftSwitchesComeUpInTextMode)
    {
        MachineHost           host;
        MachineBuildServices  nothingListening;
        MachineBuilder        builder (host, nothingListening);
        MachineConfig         config;

        Build (host, builder, IDR_MACHINE_APPLE2E, "Apple2e", config);

        host.PowerCycle();
        host.RunCycles (s_kBootCycles);

        //  The reset routine puts the display back to 40-column text. Every
        //  one of these reads the live bank rather than the latched mirror,
        //  which only the frame loop writes.
        Assert::IsFalse (host.GetRefs().softSwitches->IsGraphicsMode(),
            L"a machine that has just reset shows text, not graphics");
        Assert::IsFalse (host.GetRefs().iieSoftSwitches->Is80ColMode(),
            L"and 40 columns of it");
    }


    TEST_METHOD (MainRamIsWritableAcrossItsWholeRange)
    {
        MachineHost           host;
        MachineBuildServices  nothingListening;
        MachineBuilder        builder (host, nothingListening);
        MachineConfig         config;

        Build (host, builder, IDR_MACHINE_APPLE2E, "Apple2e", config);

        //  The page table is what routes these, so a wrong one shows up as a
        //  read that does not answer with what was written.
        for (Word addr : { Word (0x0000), Word (0x0400), Word (0x2000),
                           Word (0x6000), Word (0xBFFF) })
        {
            host.GetMemoryBus().WriteByte (addr, 0x5A);
            Assert::AreEqual<Byte> (0x5A, host.GetMemoryBus().ReadByte (addr),
                std::format (L"main RAM must answer at ${:04X}", addr).c_str());
        }
    }


    TEST_METHOD (OnlyASlotlessMachineGivesUpItsCxxxRange)
    {
        //  The banked-ROM wiring hands $C100-$CFFF entirely to the internal
        //  firmware when the machine has no card slots, and it asks the
        //  machine rather than assuming -- it used to assume, which made
        //  "banked ROM" and "no slots" the same fact for anyone who added a
        //  banked machine later.
        //
        //  The //c is the one machine that declares zero, so this is the
        //  fact the wiring reads, asserted where a change to either side
        //  shows up.
        Assert::AreEqual (0, MachineDefinitions::Find ("Apple2c")->slotCount,
            L"the //c has no card slots");

        for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
        {
            Assert::AreEqual (7, MachineDefinitions::Find (id)->slotCount,
                L"every other model has seven");
        }
    }


    TEST_METHOD (EveryMachineTheFixturesCanSupplyBuilds)
    {
        //  Through the production builder, from the shipped JSON. A machine
        //  whose config asks for a device the builder cannot make fails here
        //  rather than on someone's desk.
        //
        //  The ][ and ][+ are absent, and it is the ROMs rather than the
        //  machines: the committed fixture set is Apple2e.rom,
        //  Apple2eEnhanced.rom, Apple2c.rom, Apple2e_Video.rom and Disk2.rom,
        //  so those two have neither a system nor a character ROM here. The
        //  loader resolves both before it returns, so they cannot be loaded
        //  at all rather than loading and rendering wrong. Committing their
        //  ROMs is the only thing between them and this list -- and they
        //  have no emulation coverage of any kind today, so it would be new
        //  ground rather than a migration.
        struct Shipped
        {
            int          resourceId;
            const char * machineName;
        };

        const Shipped  machines[] =
        {
            { IDR_MACHINE_APPLE2E,          "Apple2e"         },
            { IDR_MACHINE_APPLE2E_ENHANCED, "Apple2eEnhanced" },
            { IDR_MACHINE_APPLE2C,          "Apple2c"         },
        };

        for (const Shipped & m : machines)
        {
            MachineHost           host;
            MachineBuildServices  nothingListening;
            MachineBuilder        builder (host, nothingListening);
            MachineConfig         config;
            std::wstring          name (m.machineName, m.machineName + strlen (m.machineName));

            Build (host, builder, m.resourceId, m.machineName, config);

            Assert::IsNotNull (host.GetCpu(),
                std::format (L"{} must build a CPU", name).c_str());
            Assert::IsNotNull (host.GetRefs().keyboard,
                std::format (L"{} must build a keyboard", name).c_str());
            Assert::IsNotNull (host.GetRefs().speaker,
                std::format (L"{} must build a speaker", name).c_str());

            host.PowerCycle();
            host.RunCycles (s_kBootCycles);

            Assert::IsTrue (host.GetCpu()->GetPC() >= 0xC000,
                std::format (L"{} must boot into ROM; PC was ${:04X}",
                             name, host.GetCpu()->GetPC()).c_str());
        }
    }


private:

    static void Build (MachineHost    & host,
                       MachineBuilder & builder,
                       int              resourceId,
                       const char *     machineName,
                       MachineConfig  & config)
    {
        std::wstring  wide (machineName, machineName + strlen (machineName));
        HRESULT       hr = S_OK;

        LoadConfig (resourceId, machineName, config);

        host.SetPrng (std::make_unique<Prng> (s_kSeed));
        host.SetCurrentMachineName (wide);
        host.GetConfig() = config;

        hr = builder.Build (config);
        AssertSucceeded (hr, std::format (L"{} must build", wide).c_str());
    }


    //  The shipped JSON, through the shipped loader, with ROM files resolved
    //  out of the fixtures directory instead of an installed Resources tree.
    //
    //  Fixtures hold the ROMs flat, so the resolver ignores the per-machine
    //  and per-device subdirectories the production one walks and answers on
    //  the file name alone. That is the whole reason the loader takes a
    //  resolver: where a ROM lives is the host's business, not the config's.
    static void LoadConfig (int resourceId, const char * machineName, MachineConfig & config)
    {
        FixtureProvider  fixtures;
        fs::path         root     = fs::path (fixtures.GetRoot());
        std::string      jsonText = EmbeddedMachineJson::Load (resourceId);
        std::string      error;
        HRESULT          hr       = S_OK;

        auto  resolveFlat = [root] (const std::vector<fs::path> &,
                                    const fs::path & romRelPath) -> fs::path
        {
            fs::path  name      = romRelPath.filename();
            fs::path  candidate = root / name;

            //  The //c uses the //e's character generator -- the two files
            //  are the same bytes under two names -- so the fixtures carry
            //  it once and this answers for both.
            if (!fs::exists (candidate) && name == L"Apple2c_Video.rom")
            {
                candidate = root / L"Apple2e_Video.rom";
            }

            return (fs::exists (candidate) ? candidate : fs::path());
        };

        hr = MachineConfigLoader::Load (jsonText,
                                        machineName,
                                        { root },
                                        resolveFlat,
                                        config,
                                        error);

        AssertSucceeded (hr,
            std::format (L"{} config must load: {}",
                         std::wstring (machineName, machineName + strlen (machineName)),
                         std::wstring (error.begin(), error.end())).c_str());
    }
};
