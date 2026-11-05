#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"
#include "Debugger/MachineDebugTarget.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/CassetteDiagnostics.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/Ssi263AudioSource.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsProviderTests
//
//  What each device publishes for its debugger panel: named groups of rows,
//  bit decodes, and the payloads the panel graphics draw. Every provider is
//  read on a real machine where it can be, so a row reports the state the
//  machine is really in.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiagnosticsProviderTests)
    {
    public:

        static inline const DiagnosticsRow  kMissing;
        static inline const std::string     kMixer = "Mixer (lit = enabled)";


        static DiagnosticsSnapshot Snapshot (const IDiagnosticsProvider & provider)
        {
            DiagnosticsSnapshot  snapshot;



            provider.GetDiagnostics (snapshot);
            return snapshot;
        }


        //  The first row with this label in any group; a test that asks for a
        //  row the provider does not publish fails here rather than reading an
        //  empty one.
        static const DiagnosticsRow & FindRow (const DiagnosticsSnapshot & snapshot, const std::string & label)
        {
            for (const DiagnosticsGroup & group : snapshot.groups)
            {
                for (const DiagnosticsRow & row : group.rows)
                {
                    if (row.label == label)
                    {
                        return row;
                    }
                }
            }

            Assert::Fail ((L"no row " + TextEncoding::NarrowToWide (label)).c_str());
            return kMissing;
        }


        static bool IsBitSet (const DiagnosticsRow & row, const std::string & name)
        {
            for (const DiagnosticsBit & bit : row.bits)
            {
                if (bit.name == name)
                {
                    return bit.set;
                }
            }

            Assert::Fail ((L"no bit " + TextEncoding::NarrowToWide (name)).c_str());
            return false;
        }


        static std::vector<std::string> GetIds (const MachineHost & machine)
        {
            std::vector<std::string>  ids;



            for (const IDiagnosticsProvider * provider : machine.GetDiagnosticsProviders())
            {
                ids.push_back (provider->GetDiagnosticsId());
            }

            return ids;
        }


        static const IDiagnosticsProvider * FindProvider (const MachineHost & machine, const std::string & id)
        {
            for (const IDiagnosticsProvider * provider : machine.GetDiagnosticsProviders())
            {
                if (provider->GetDiagnosticsId() == id)
                {
                    return provider;
                }
            }

            return nullptr;
        }


        static void EnableSlotDevice (MachineConfig & config, const char * device)
        {
            for (SlotConfig & slot : config.slots)
            {
                if (slot.device == device)
                {
                    slot.enabled = true;
                }
            }
        }


        static bool Contains (const std::vector<std::string> & ids, const std::string & id)
        {
            return std::find (ids.begin(), ids.end(), id) != ids.end();
        }



        TEST_METHOD (AByteRowDecodesEachNamedBitHighestFirst)
        {
            DiagnosticsRow  row = IDiagnosticsProvider::MakeByteRow ("R", 0xA1, { "B7", "B6", "", "B4", "", "", "", "B0" });



            Assert::AreEqual (std::string ("$A1"), row.value);
            Assert::AreEqual ((size_t) 4, row.bits.size(), L"an empty name leaves its bit out");
            Assert::AreEqual (std::string ("B7"), row.bits[0].name);
            Assert::IsTrue   (row.bits[0].set);
            Assert::IsFalse  (row.bits[1].set, L"B6");
            Assert::IsFalse  (row.bits[2].set, L"B4");
            Assert::IsTrue   (row.bits[3].set, L"B0");
        }


        TEST_METHOD (TheDiskPanelFollowsASeek)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::DiskOnly);
            Disk2Controller            * disk   = machine.GetRefs().diskController;
            DiagnosticsSnapshot          before;
            DiagnosticsSnapshot          after;
            const DiagnosticsDiskHead  * head   = nullptr;



            Assert::IsNotNull (disk);
            before = Snapshot (*disk);

            Assert::AreEqual (std::string ("idle"), FindRow (before, "Activity").value);
            Assert::AreEqual (std::string ("0"),   FindRow (before, "Quarter track").value);

            //  Motor on, then phase 1 on: the head steps toward it.
            (void) machine.GetMemoryBus().ReadByte (0xC0E9);
            (void) machine.GetMemoryBus().ReadByte (0xC0E3);
            after = Snapshot (*disk);

            Assert::AreEqual (std::string ("reading"), FindRow (after, "Activity").value, L"the motor is on");
            Assert::AreNotEqual (FindRow (before, "Quarter track").value, FindRow (after, "Quarter track").value, L"the head moved");
            Assert::AreEqual (std::format ("{}", disk->GetQuarterTrack()), FindRow (after, "Quarter track").value);
            Assert::IsTrue   (IsBitSet (FindRow (after, "Phases"), "PH1"));
            Assert::IsFalse  (IsBitSet (FindRow (after, "Phases"), "PH0"));
            Assert::AreEqual (std::string ("read"), FindRow (after, "Mode").value);
            Assert::AreNotEqual (FindRow (before, "Spin-up left").value, FindRow (after, "Spin-up left").value, L"spin-up starts with the motor");

            head = std::get_if<DiagnosticsDiskHead> (&after.visual);
            Assert::IsNotNull (head, L"the head graphic's payload");
            Assert::AreEqual  (disk->GetQuarterTrack(), head->quarterTrack);
            Assert::IsTrue    (head->motorOn);
            Assert::AreEqual  ((int) Disk2Controller::kMaxQuarterTrack, head->maxQuarterTrack);
            Assert::AreEqual  ((Byte) 0x02, head->phases);
        }


        TEST_METHOD (TheMmuPanelListsEverySwitchAndMapsEveryPage)
        {
            TestMachine                  machine  ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot          snapshot = Snapshot (*machine.GetMmu());
            const DiagnosticsMemoryMap * map      = std::get_if<DiagnosticsMemoryMap> (&snapshot.visual);



            Assert::IsTrue (std::none_of (snapshot.groups.begin(), snapshot.groups.end(), [] (const DiagnosticsGroup & group)
            {
                return std::any_of (group.rows.begin(), group.rows.end(), [] (const DiagnosticsRow & row) { return row.label == "MMU"; });
            }), L"no packed row: each switch is listed once");

            for (const char * name : { "RAMRD", "RAMWRT", "ALTZP", "80STORE", "INTCXROM", "SLOTC3ROM", "INTC8ROM" })
            {
                (void) FindRow (snapshot, name);
            }

            Assert::IsNotNull (map);
            Assert::AreEqual  (DiagnosticsMemoryMap::kPageCount, map->pages.size());
            Assert::IsTrue    (map->pages[0x00].read  == MemorySource::Main);
            Assert::IsTrue    (map->pages[0x20].write == MemorySource::Main);
            Assert::IsTrue    (map->pages[0xC0].read  == MemorySource::Io);
            Assert::IsTrue    (map->pages[0xE0].read  == MemorySource::Rom || map->pages[0xE0].read == MemorySource::LcBank2);
        }


        //  ALTZP moves the zero page and the stack to aux, and back.
        TEST_METHOD (TheMemoryMapChangesOnAltZp)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  on;
            DiagnosticsSnapshot  off;



            machine.GetMemoryBus().WriteByte (0xC009, 0);
            on = Snapshot (*machine.GetMmu());

            Assert::AreEqual (std::string ("on"), FindRow (on, "ALTZP").value);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x00].read  == MemorySource::Aux);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x01].write == MemorySource::Aux);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x02].read  == MemorySource::Main, L"only pages 0 and 1");

            machine.GetMemoryBus().WriteByte (0xC008, 0);
            off = Snapshot (*machine.GetMmu());

            Assert::AreEqual (std::string ("off"), FindRow (off, "ALTZP").value);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (off.visual).pages[0x00].read == MemorySource::Main);
        }


        //  $C800-$CEFF is the ROM of the card that an access to its $Cs00
        //  page selected, and nothing once $CFFF deselects it.
        TEST_METHOD (TheMemoryMapHasTheSlotWhoseRomIsAtC800)
        {
            TestMachine           machine ("Apple2e", [] (MachineConfig & config) { EnableSlotDevice (config, kpszSuperSerialCardDevice); });
            MachineDebugTarget    target  (machine);
            DiagnosticsMemoryMap  map;
            int                   slot    = -1;



            machine.GetMemoryBus().ReadByte (0xC200);
            map = std::get<DiagnosticsMemoryMap> (Snapshot (*machine.GetMmu()).visual);

            Assert::AreEqual (2, map.expansionSlot);
            Assert::IsTrue   (map.pages[0xC8].read == MemorySource::SlotRom);
            Assert::IsTrue   (map.pages[0xCE].read == MemorySource::SlotRom);
            Assert::IsTrue   (map.pages[0xCF].read == MemorySource::None, L"$CF00-$CFFF deselects the card");
            Assert::IsTrue   (target.TryGetExpansionRomSlot (slot));
            Assert::AreEqual (2, slot);

            machine.GetMemoryBus().ReadByte (0xCFFF);
            map = std::get<DiagnosticsMemoryMap> (Snapshot (*machine.GetMmu()).visual);

            Assert::AreEqual (0, map.expansionSlot);
            Assert::IsTrue   (map.pages[0xC8].read == MemorySource::None, L"no card drives $C800");
            Assert::IsTrue   (target.TryGetExpansionRomSlot (slot));
            Assert::AreEqual (0, slot);
        }


        //  The //c has no slots, so its whole $C100-$CFFF is internal ROM.
        TEST_METHOD (TheIIcMapsItsCxxxSpaceToItsRom)
        {
            TestMachine           machine ("Apple2c");
            DiagnosticsMemoryMap  map     = std::get<DiagnosticsMemoryMap> (Snapshot (*machine.GetMmu()).visual);



            for (size_t page : { 0xC1, 0xC3, 0xC8, 0xCF })
            {
                Assert::IsTrue (map.pages[page].read == MemorySource::Rom, std::format (L"page ${:02X}", page).c_str());
            }
        }


        TEST_METHOD (TheVideoPanelNamesTheMode)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            AppleSoftSwitchBank  & video    = *machine.GetRefs().softSwitches;
            DiagnosticsSnapshot    snapshot;



            machine.GetMemoryBus().WriteByte (0xC051, 0);   // TEXT
            snapshot = Snapshot (video);
            Assert::AreEqual (std::string ("text"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),   FindRow (snapshot, "TEXT").value);

            machine.GetMemoryBus().WriteByte (0xC050, 0);   // graphics
            machine.GetMemoryBus().WriteByte (0xC057, 0);   // hi-res
            machine.GetMemoryBus().WriteByte (0xC053, 0);   // mixed
            machine.GetMemoryBus().WriteByte (0xC055, 0);   // page 2
            snapshot = Snapshot (video);

            Assert::AreEqual (std::string ("hi-res, mixed"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),            FindRow (snapshot, "PAGE2").value);
            Assert::AreEqual (std::string ("off"),           FindRow (snapshot, "80COL").value, L"the //e's switches follow");

            machine.GetMemoryBus().WriteByte (0xC00D, 0);   // 80COL
            machine.GetMemoryBus().WriteByte (0xC05E, 0);   // DHIRES
            machine.GetMemoryBus().WriteByte (0xC052, 0);   // full screen
            snapshot = Snapshot (video);

            Assert::AreEqual (std::string ("double hi-res"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),            FindRow (snapshot, "DHIRES").value);
        }


        TEST_METHOD (TheIIPlusVideoPanelHasNoIIeSwitches)
        {
            TestMachine          machine  ("Apple2Plus", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  snapshot = Snapshot (*machine.GetRefs().softSwitches);



            Assert::AreEqual ((size_t) 1, snapshot.groups.size());
            Assert::AreEqual (std::string ("Display"), snapshot.groups[0].title);
        }


        TEST_METHOD (TheKeyboardPanelShowsTheLatchWithoutClearingIt)
        {
            TestMachine          machine  ("Apple2e", TestMachine::Slots::Empty);
            AppleKeyboard      & keyboard = *machine.GetRefs().keyboard;
            DiagnosticsSnapshot  snapshot;



            keyboard.PressKey ('A');
            snapshot = Snapshot (keyboard);

            Assert::AreEqual (std::string ("$C1"), FindRow (snapshot, "Latch ($C000)").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Latch ($C000)"), "STROBE"));
            Assert::AreEqual (std::string ("'A'"), FindRow (snapshot, "Key").value);
            Assert::AreEqual (std::string ("on"),  FindRow (snapshot, "Key pending").value);
            Assert::IsFalse  (keyboard.IsStrobeClear(), L"reading the panel left the strobe set");
        }


        //  Each 6522 and each AY as a group, and meters for the six channels and
        //  the four timers.
        TEST_METHOD (TheMockingboardPanelCoversBothHalves)
        {
            MockingboardCard     card (4);
            DiagnosticsSnapshot       snapshot;
            std::vector<std::string>  titles;



            snapshot = Snapshot (card);

            for (const DiagnosticsGroup & group : snapshot.groups)
            {
                titles.push_back (group.title);
            }

            Assert::IsTrue   (Contains (titles, "6522 #1") && Contains (titles, "6522 #2"));
            Assert::IsTrue   (Contains (titles, "AY #1")   && Contains (titles, "AY #2"));
            Assert::IsTrue   (std::holds_alternative<DiagnosticsSoundBoard> (snapshot.visual));
            Assert::AreEqual ((size_t) 2, std::get<DiagnosticsSoundBoard> (snapshot.visual).halves.size());
            Assert::IsFalse  (std::get<DiagnosticsSoundBoard> (snapshot.visual).voice.has_value(), L"a Mockingboard A has no voice");
            Assert::IsFalse  (Contains (titles, "SSI-263"), L"a Mockingboard A has no speech chip");
            (void) FindRow (snapshot, "IFR");
            (void) FindRow (snapshot, "T1 count");
            (void) FindRow (snapshot, kMixer);
        }


        TEST_METHOD (AMockingboardCPublishesItsSpeechChip)
        {
            MockingboardCard      card (4, MockingboardVariant::SoundSpeech);
            DiagnosticsSnapshot   snapshot     = Snapshot (card);
            bool                  hasGroup     = false;



            for (const DiagnosticsGroup & group : snapshot.groups)
            {
                hasGroup = hasGroup || (group.title == "SSI-263");
            }

            Assert::IsTrue   (hasGroup);
            Assert::IsTrue   (std::get<DiagnosticsSoundBoard> (snapshot.visual).voice.has_value());
            Assert::AreEqual (std::string ("Down"), FindRow (snapshot, "Power").value, L"power down is the chip's state after a reset");
        }


        //  The clock and period make a round frequency, and the envelope a round
        //  ramp: 1000000 / (16 * 100) and 256 * 500 / 1000000.
        TEST_METHOD (TheAyPublishesItsChannelsForTheSoundBoard)
        {
            Ay8910               ay (1000000.0);
            DiagnosticsSoundPsg  psg;



            ay.WriteRegister (Ay8910::kRegToneAFine,  100);
            ay.WriteRegister (Ay8910::kRegMixer,      0x36);     // tone A and noise A on, the rest off
            ay.WriteRegister (Ay8910::kRegAmpA,       0x0C);
            ay.WriteRegister (Ay8910::kRegAmpB,       0x10);     // B from the envelope
            ay.WriteRegister (Ay8910::kRegEnvFine,    0xF4);
            ay.WriteRegister (Ay8910::kRegEnvCoarse,  0x01);
            ay.WriteRegister (Ay8910::kRegNoisePeriod, 10);
            ay.AppendSoundBoard (psg);

            Assert::AreEqual (625.0,  psg.channels[0].frequencyHz, 0.001);
            Assert::AreEqual (12,     psg.channels[0].level);
            Assert::IsTrue   (psg.channels[0].isToneOn);
            Assert::IsTrue   (psg.channels[0].isNoiseOn);
            Assert::IsFalse  (psg.channels[1].isToneOn);
            Assert::IsFalse  (psg.channels[1].isNoiseOn);
            Assert::IsTrue   (psg.channels[1].usesEnvelope);
            Assert::AreEqual (psg.envelopeLevel, psg.channels[1].level);
            Assert::AreEqual (128.0,  psg.envelopeRampMs, 0.001);
            Assert::AreEqual (6250.0, psg.noiseHz, 0.001);
        }


        //  A write by the bus counts; one by the direct path tests use does not.
        TEST_METHOD (TheAyCountsItsBusWritesAndHasTheLast)
        {
            Ay8910               ay;
            DiagnosticsSoundPsg  before;
            DiagnosticsSoundPsg  after;



            ay.WriteRegister (Ay8910::kRegAmpA, 1);
            ay.AppendSoundBoard (before);

            ay.LatchAddress (9);
            ay.WriteData    (0x04);
            ay.LatchAddress (10);
            ay.WriteData    (0x05);
            ay.AppendSoundBoard (after);

            Assert::IsFalse  (before.hasLastWrite);
            Assert::AreEqual ((uint32_t) 2, after.writeCount);
            Assert::IsTrue   (after.hasLastWrite);
            Assert::AreEqual ((int) 10,   (int) after.lastWriteReg);
            Assert::AreEqual ((int) 0x05, (int) after.lastWriteValue);
        }


        //  With every source off a channel's output is its level, so the scope and
        //  the mix are exact: A at full, 1.0, and B at level 11, 0.25.
        TEST_METHOD (AMuteLeavesTheMixAndTheScopeHasTheSignal)
        {
            constexpr uint32_t   kSampleRate = 44100;
            Ay8910               ay;
            DiagnosticsSoundPsg  psg;
            float                both        = 0.0f;
            float                muted       = 0.0f;



            ay.SetSampleRate (kSampleRate);
            ay.WriteRegister (Ay8910::kRegMixer, 0x3F);
            ay.WriteRegister (Ay8910::kRegAmpA,  0x0F);
            ay.WriteRegister (Ay8910::kRegAmpB,  0x0B);

            both = ay.GenerateSample();

            ay.AppendSoundBoard (psg);
            ay.SetChannelMuted  (0, true);
            muted = ay.GenerateSample();
            ay.AppendSoundBoard (psg);

            Assert::AreEqual (1.25f, both,  0.0001f);
            Assert::AreEqual (0.25f, muted, 0.0001f);
            Assert::IsTrue   (psg.channels[0].isMuted);
            Assert::AreEqual (Ay8910::kScopeSamples, psg.channels[0].scope.size());
            Assert::AreEqual (1.0f,  psg.channels[0].scope.back(), 0.0001f, L"the muted channel's signal, newest last");
            Assert::AreEqual (0.25f, psg.channels[1].scope.back(), 0.0001f);
            Assert::AreEqual (0.0f,  psg.channels[2].scope.back(), 0.0001f);
        }


        TEST_METHOD (AnAyResetByTheGuestLeavesTheMute)
        {
            Ay8910  ay;



            ay.SetChannelMuted (1, true);
            ay.Reset();

            Assert::IsTrue (ay.IsChannelMuted (1));
        }


        //  The panel calls this once a frame, and not once closed, so capture
        //  stops after its time runs out.
        TEST_METHOD (ScopeCaptureRunsOnlyWhileThePanelAsks)
        {
            constexpr uint32_t   kSampleRate = 1000;
            Ay8910               ay;
            DiagnosticsSoundPsg  psg;
            uint32_t             i           = 0;



            ay.SetSampleRate (kSampleRate);
            Assert::IsFalse  (ay.IsScopeCapturing(), L"no call yet");

            ay.AppendSoundBoard (psg);
            Assert::IsTrue   (ay.IsScopeCapturing());

            for (i = 0; i < kSampleRate * Ay8910::kScopeCaptureSeconds; i++)
            {
                (void) ay.GenerateSample();
            }

            Assert::IsFalse  (ay.IsScopeCapturing(), L"the capture time has run out");
        }


        TEST_METHOD (TheSpeechChipPublishesItsVoiceForTheSoundBoard)
        {
            constexpr uint32_t     kSampleRate = 1000;
            Ssi263                 chip;
            DiagnosticsSoundVoice  voice;
            uint32_t               i           = 0;



            chip.SetSampleRate (kSampleRate);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x80);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC0);
            chip.WriteRegister (Ssi263::kRegRateInflection,  0xA8);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x5C);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC1);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC2);
            chip.AppendSoundBoard (voice);

            Assert::AreEqual (std::string ("E1 (bent)"),            voice.phoneme);
            Assert::AreEqual (std::string ("Phoneme transitioned"), voice.mode);
            Assert::IsTrue   (voice.isSounding);
            Assert::IsFalse  (voice.isPoweredDown);
            Assert::AreEqual ((size_t) 3, voice.recent.size(), L"the one leaving power down starts, then the two");
            Assert::AreEqual (std::string ("PA (pause)"), voice.recent[0].name);
            Assert::AreEqual (std::string ("E (meet)"),   voice.recent[1].name);
            Assert::AreEqual (std::string ("E1 (bent)"),  voice.recent[2].name);
            Assert::AreEqual (0.0137f, voice.recent[2].seconds, 0.0001f);

            Assert::IsTrue   (chip.IsScopeCapturing());

            for (i = 0; i < kSampleRate * Ssi263::kScopeCaptureSeconds; i++)
            {
                (void) chip.GenerateSample();
            }

            Assert::IsFalse  (chip.IsScopeCapturing());
            chip.AppendSoundBoard (voice);
            Assert::AreEqual (Ssi263::kScopeSamples, voice.scope.size());
        }


        TEST_METHOD (TheRecentPhonemesKeepTheNewest)
        {
            Ssi263                 chip;
            DiagnosticsSoundVoice  voice;
            int                    i = 0;



            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x5C);

            for (i = 0; i < Ssi263::kRecentCount + 3; i++)
            {
                chip.WriteRegister (Ssi263::kRegDurationPhoneme, (Byte) (i & Ssi263::kPhonemeMask));
            }

            chip.AppendSoundBoard (voice);

            Assert::AreEqual ((size_t) Ssi263::kRecentCount, voice.recent.size());
            Assert::AreEqual (std::string (Ssi263::GetPhonemeName (3)), voice.recent.front().name);
            Assert::AreEqual (std::string (Ssi263::GetPhonemeName ((Byte) (Ssi263::kRecentCount + 2))), voice.recent.back().name);
        }


        //  The same phoneme written again while it sounds is one entry, as
        //  long as its sound so far plus the new duration; another phoneme
        //  between makes a new one.
        TEST_METHOD (APhonemeRewrittenWhileItSoundsIsOneEntry)
        {
            Ssi263                 chip;
            DiagnosticsSoundVoice  voice;
            float                  one = 0.0f;



            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x5C);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x05);
            chip.AppendSoundBoard (voice);
            one = voice.recent.back().seconds;

            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x05);
            chip.AppendSoundBoard (voice);

            Assert::AreEqual ((size_t) 1, voice.recent.size(), L"a rewrite is the same entry");
            Assert::AreEqual (one, voice.recent.back().seconds, one * 0.01f, L"no sound so far, plus the new duration");

            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x06);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x05);
            chip.AppendSoundBoard (voice);
            Assert::AreEqual ((size_t) 3, voice.recent.size(), L"another phoneme between is a new entry");
        }


        //  A muted source is silent and the chip renders as it would otherwise.
        TEST_METHOD (AMutedSpeechSourceIsSilentAndTheChipRendersOn)
        {
            constexpr uint32_t  kSampleRate = 44100;
            constexpr uint32_t  kSamples    = 4096;
            Ssi263              audible;
            Ssi263              muted;
            Ssi263AudioSource   audibleSource;
            Ssi263AudioSource   mutedSource;
            std::vector<float>  audibleOut (kSamples);
            std::vector<float>  mutedOut (kSamples);
            float               peak        = 0.0f;
            uint32_t            i           = 0;



            for (Ssi263 * chip : { &audible, &muted })
            {
                chip->SetSampleRate (kSampleRate);
                chip->WriteRegister (Ssi263::kRegCtlArtAmp,       0x80);
                chip->WriteRegister (Ssi263::kRegDurationPhoneme, 0xC0);
                chip->WriteRegister (Ssi263::kRegCtlArtAmp,       0x0F);
                chip->WriteRegister (Ssi263::kRegDurationPhoneme, 0xC1);
            }

            audibleSource.SetSpeech (&audible);
            mutedSource.SetSpeech (&muted);
            mutedSource.SetMuted  (true);

            audibleSource.GeneratePCM (audibleOut.data(), kSamples);
            mutedSource.GeneratePCM (mutedOut.data(), kSamples);

            for (i = 0; i < kSamples; i++)
            {
                Assert::AreEqual (0.0f, mutedOut[i]);
                peak = std::max (peak, std::abs (audibleOut[i]));
            }

            Assert::IsTrue   (peak > 0.0f, L"the chip was sounding");
            Assert::AreEqual (audible.GenerateSample(), muted.GenerateSample(), L"the same state after the same samples");
        }


        //  1000000 / (0x00FE + 2) for T1, free running, with its IFR and IER bits.
        TEST_METHOD (TheViaPublishesItsRateAndLampsForTheSoundBoard)
        {
            constexpr double     kClockHz = 1024000.0;
            Via6522              via;
            DiagnosticsSoundVia  idle;
            DiagnosticsSoundVia  running;



            via.AppendSoundBoard (kClockHz, idle);

            via.WriteRegister (Via6522::kRegAcr,  Via6522::kAcrT1Continuous);
            via.WriteRegister (Via6522::kRegT1LL, 0xFE);
            via.WriteRegister (Via6522::kRegT1CH, 0x00);
            via.WriteRegister (Via6522::kRegIer,  Via6522::kIerSetClear | Via6522::kIrqTimer1);
            via.Tick (0x0200);
            via.AppendSoundBoard (kClockHz, running);

            Assert::AreEqual (0.0,  idle.t1RateHz);
            Assert::IsFalse  (idle.isRunning);
            Assert::IsFalse  (idle.isFired);
            Assert::AreEqual (4000.0, running.t1RateHz, 0.001);
            Assert::IsTrue   (running.isRunning);
            Assert::IsTrue   (running.isFired);
            Assert::IsTrue   (running.isT1Irq);
            Assert::IsTrue   (running.isIrq);
            Assert::AreEqual ((int) (uint16_t) 0x00FE, (int) running.t1Latch);
        }


        //  The bus state of each half, from port B's lines, and the half's address.
        TEST_METHOD (TheBoardPublishesEachHalfsBusStateAndAddress)
        {
            MockingboardCard       card (4);
            DiagnosticsSnapshot    snapshot;
            DiagnosticsSoundBoard  board;



            card.Write (0xC402, 0x07);      // DDRB
            card.Write (0xC400, 0x06);      // reset high, write
            snapshot = Snapshot (card);
            board    = std::get<DiagnosticsSoundBoard> (snapshot.visual);

            Assert::AreEqual (4, board.slot);
            Assert::AreEqual ((Word) 0xC400, board.halves[0].baseAddress);
            Assert::AreEqual ((Word) 0xC480, board.halves[1].baseAddress);
            Assert::AreEqual (std::string ("Write"), board.halves[0].busState);
            Assert::AreEqual (std::string ("Reset"), board.halves[1].busState);
        }


        TEST_METHOD (TheCardSetsEachMute)
        {
            MockingboardCard       card (4, MockingboardVariant::SoundSpeech);
            DiagnosticsSoundBoard  board;
            DiagnosticsSnapshot    snapshot;



            card.SetSoundMuted (0, true);
            card.SetSoundMuted (5, true);
            card.SetSoundMuted (MockingboardCard::kSpeechMuteChannel, true);
            snapshot = Snapshot (card);
            board    = std::get<DiagnosticsSoundBoard> (snapshot.visual);

            Assert::IsTrue  (board.halves[0].psg.channels[0].isMuted);
            Assert::IsFalse (board.halves[0].psg.channels[1].isMuted);
            Assert::IsTrue  (board.halves[1].psg.channels[2].isMuted);
            Assert::IsTrue  (board.voice->isMuted);
        }


        //  At the datasheet's XCK: frame = 4096 * (16 - 10) / XCK, phoneme =
        //  frame * (4 - 3), inflection = XCK / (8 * (4096 - $800)), and
        //  filter = XCK / (2 * (256 - $E7)).
        TEST_METHOD (TheSpeechChipPublishesItsPhonemeAndTiming)
        {
            Ssi263               chip;
            DiagnosticsSnapshot  snapshot;



            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x80);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC0);
            chip.WriteRegister (Ssi263::kRegRateInflection,  0xA8);
            chip.WriteRegister (Ssi263::kRegInflection,      0x00);
            chip.WriteRegister (Ssi263::kRegFilterFreq,      0xE7);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x5C);     // out of power down, which latches the mode
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC1);
            chip.AppendDiagnostics ("SSI-263", snapshot);

            Assert::AreEqual (std::string ("On"),                   FindRow (snapshot, "Power").value);
            Assert::AreEqual (std::string ("on"),                   FindRow (snapshot, "Sounding").value);
            Assert::AreEqual (std::string ("off"),                  FindRow (snapshot, "Requesting").value);
            Assert::AreEqual (std::string ("Phoneme transitioned"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("$01 E (meet)"),         FindRow (snapshot, "Phoneme").value);
            Assert::AreEqual (std::string ("$3 (13.7 ms)"),         FindRow (snapshot, "Duration").value);
            Assert::AreEqual (std::string ("$A (13.7 ms a frame)"), FindRow (snapshot, "Rate").value);
            Assert::AreEqual (std::string ("$800 (109 Hz)"),        FindRow (snapshot, "Inflection").value);
            Assert::AreEqual (std::string ("$5"),                   FindRow (snapshot, "Articulation").value);
            Assert::AreEqual (std::string ("$C"),                   FindRow (snapshot, "Amplitude").value);
            Assert::AreEqual (std::string ("$E7 (35795 Hz)"),       FindRow (snapshot, "Filter").value);
        }


        TEST_METHOD (EverySpeechPhonemeHasAName)
        {
            Ssi263  chip;



            for (int phoneme = 0; phoneme < Ssi263::kPhonemeCount; phoneme++)
            {
                DiagnosticsSnapshot  snapshot;
                std::string          prefix  = std::format ("${:02X} ", phoneme);
                std::string          value;



                chip.WriteRegister (Ssi263::kRegDurationPhoneme, (Byte) phoneme);
                chip.AppendDiagnostics ("SSI-263", snapshot);
                value = FindRow (snapshot, "Phoneme").value;

                Assert::IsTrue (value.starts_with (prefix) && value.size() > prefix.size(), TextEncoding::NarrowToWide (value).c_str());
            }
        }


        //  The formants row is the centers the resonators filter at. This XCK and
        //  filter register put the filter at its nominal clock, for a scale of 1.
        TEST_METHOD (TheSpeechChipPublishesItsFormantsAndLevel)
        {
            constexpr double     kXckHz      = 8000000.0;
            constexpr Byte       kNominal    = 56;
            constexpr uint32_t   kSampleRate = 44100;
            Ssi263               chip (kXckHz);
            DiagnosticsSnapshot    cold;
            DiagnosticsSnapshot    sounding;
            DiagnosticsSoundVoice  voice;



            chip.AppendDiagnostics ("SSI-263", cold);
            Assert::AreEqual (std::string ("0, 0, 0 Hz"), FindRow (cold, "Formants").value);

            chip.SetSampleRate (kSampleRate);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x80);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC0);
            chip.WriteRegister (Ssi263::kRegFilterFreq,      kNominal);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp,       0x0F);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0xC1);

            for (uint32_t sample = 0; sample < kSampleRate / 10; sample++)
            {
                (void) chip.GenerateSample();
            }

            chip.AppendDiagnostics ("SSI-263", sounding);
            chip.AppendSoundBoard (voice);

            Assert::AreEqual (std::format ("{:.0f}, {:.0f}, {:.0f} Hz", chip.GetFormantCenter (0), chip.GetFormantCenter (1), chip.GetFormantCenter (2)),
                              FindRow (sounding, "Formants").value);
            Assert::AreEqual (314.0, chip.GetFormantCenter (0), 1.0, L"the glide is at E's first formant");
            Assert::AreEqual (chip.GetFormantCenter (0), voice.formantsHz[0], 0.001, L"at a scale of 1");
            Assert::IsTrue   (voice.level > 0.5f, L"a sounding phoneme at full amplitude");
        }


        TEST_METHOD (TheViaPublishesItsTimersAndInterruptRegisters)
        {
            Via6522              via;
            DiagnosticsSnapshot  snapshot;
            DiagnosticsMeters    meters;



            via.WriteRegister (Via6522::kRegT1LL, 0x00);
            via.WriteRegister (Via6522::kRegT1CH, 0x80);     // T1 = $8000, and started
            via.WriteRegister (Via6522::kRegIer,  0xC0);     // enable T1
            via.AppendDiagnostics ("6522", snapshot);
            via.AppendTimerLevels ("6522", meters);

            Assert::AreEqual (std::string ("$8000"), FindRow (snapshot, "T1 count").value);
            Assert::AreEqual (std::string ("$8000"), FindRow (snapshot, "T1 latch").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "IER"), "T1"));
            Assert::IsFalse  (IsBitSet (FindRow (snapshot, "IER"), "T2"));
            Assert::AreEqual ((size_t) 2, meters.levels.size());
            Assert::AreEqual (1.0f, meters.levels[0].level, 0.001f, L"a full count against its latch");

            via.Tick (0x4000);
            meters.levels.clear();
            via.AppendTimerLevels ("6522", meters);
            Assert::IsTrue (meters.levels[0].level > 0.4f && meters.levels[0].level < 0.6f, L"half counted down");
        }


        //  A read of IER returns bit 7 as 1, the set/clear control of a write
        //  rather than an enable, so the row shows the enables alone.
        TEST_METHOD (TheIerRowShowsTheEnablesWithoutBit7)
        {
            Via6522              via;
            DiagnosticsSnapshot  none;
            DiagnosticsSnapshot  timer1;



            via.AppendDiagnostics ("6522", none);
            via.WriteRegister (Via6522::kRegIer, 0xC0);     // enable T1
            via.AppendDiagnostics ("6522", timer1);

            Assert::AreEqual ((Byte) 0xC0, via.ReadRegister (Via6522::kRegIer), L"the register still reads bit 7 as 1");

            Assert::AreEqual (std::string ("$00"), FindRow (none, "IER").value, L"nothing enabled");
            Assert::AreEqual ((size_t) 7, FindRow (none, "IER").bits.size(), L"T1 to CA2, and no bit 7");

            for (const DiagnosticsBit & bit : FindRow (none, "IER").bits)
            {
                Assert::IsFalse (bit.set, TextEncoding::NarrowToWide (bit.name).c_str());
            }

            Assert::AreEqual (std::string ("$40"), FindRow (timer1, "IER").value);
            Assert::IsTrue   (IsBitSet (FindRow (timer1, "IER"), "T1"));
            Assert::IsFalse  (IsBitSet (FindRow (timer1, "IER"), "T2"));
            Assert::IsFalse  (IsBitSet (FindRow (timer1, "IFR"), "IRQ"), L"IFR keeps its IRQ bit, clear with no flag set");
        }


        TEST_METHOD (TheAyPublishesItsRegistersAndChannelLevels)
        {
            Ay8910               ay;
            DiagnosticsSnapshot  snapshot;
            DiagnosticsMeters    meters;



            ay.WriteRegister (Ay8910::kRegMixer, 0x3E);      // tone A only
            ay.WriteRegister (Ay8910::kRegAmpA,  0x0F);
            ay.WriteRegister (Ay8910::kRegAmpB,  0x08);
            ay.AppendDiagnostics ("AY", snapshot);
            ay.AppendChannelLevels ("AY", meters);

            Assert::IsTrue   (IsBitSet (FindRow (snapshot, kMixer), "TA"), L"tone A is on");
            Assert::IsFalse  (IsBitSet (FindRow (snapshot, kMixer), "TB"), L"a set bit turns tone B off");
            Assert::AreEqual (std::string ("$0F"), FindRow (snapshot, "Amplitude A").value);
            Assert::AreEqual ((size_t) 3, meters.levels.size());
            Assert::AreEqual (1.0f,         meters.levels[0].level, 0.001f, L"A at full amplitude");
            Assert::AreEqual (8.0f / 15.0f, meters.levels[1].level, 0.001f, L"B, with tone and noise off, holds its amplitude");
            Assert::AreEqual (0.0f,         meters.levels[2].level, 0.001f, L"C at amplitude 0");
        }


        //  The register turns a tone or noise source off with a set bit. The
        //  row's value is the register as written, and its decode is lit for a
        //  source that is on; the I/O port bits are lit for an output.
        TEST_METHOD (TheMixerRowLightsTheSourcesThatAreOn)
        {
            Ay8910               ay;
            DiagnosticsSnapshot  snapshot;
            DiagnosticsRow       mixer;



            ay.WriteRegister (Ay8910::kRegMixer, 0x6E);      // port A output, tone A and noise B on
            ay.AppendDiagnostics ("AY", snapshot);
            mixer = FindRow (snapshot, kMixer);

            Assert::AreEqual (std::string ("$6E"), mixer.value);
            Assert::AreEqual ((size_t) 8, mixer.bits.size());
            Assert::IsTrue   (IsBitSet (mixer, "TA"));
            Assert::IsFalse  (IsBitSet (mixer, "TB"));
            Assert::IsFalse  (IsBitSet (mixer, "TC"));
            Assert::IsFalse  (IsBitSet (mixer, "NA"));
            Assert::IsTrue   (IsBitSet (mixer, "NB"));
            Assert::IsFalse  (IsBitSet (mixer, "NC"));
            Assert::IsTrue   (IsBitSet (mixer, "IOA"), L"port A is an output");
            Assert::IsFalse  (IsBitSet (mixer, "IOB"), L"port B is an input");
        }


        //  With tone and noise both off a channel's output holds at its level,
        //  which is how samples are played through the amplitude register, so
        //  the meter shows that level, the same one the chip puts out.
        TEST_METHOD (AChannelWithToneAndNoiseOffMetersItsLevel)
        {
            constexpr uint32_t   kSampleRate = 44100;
            Ay8910               ay;
            DiagnosticsMeters    meters;
            float                sample      = 0.0f;



            ay.WriteRegister (Ay8910::kRegMixer, 0x3F);      // every source off
            ay.WriteRegister (Ay8910::kRegAmpB,  0x0A);
            ay.AppendChannelLevels ("AY", meters);

            ay.SetSampleRate (kSampleRate);
            sample = ay.GenerateSample();

            Assert::AreEqual (Ay8910::GetVolumeForLevel (10), sample, 0.0001f, L"the chip puts out B's level");
            Assert::AreEqual ((size_t) 3, meters.levels.size());
            Assert::AreEqual (10.0f / 15.0f, meters.levels[1].level, 0.001f, L"B's meter shows amplitude 10");
            Assert::AreEqual (0.0f,          meters.levels[0].level, 0.001f, L"A at amplitude 0");
        }


        TEST_METHOD (ThePrinterPanelShowsTheStatusTheGuestReads)
        {
            //  The card holds its print buffer, too large for the stack.
            std::unique_ptr<PrinterCard>  card     = std::make_unique<PrinterCard> (1);
            DiagnosticsSnapshot           snapshot = Snapshot (*card);



            Assert::AreEqual (std::format ("${:02X}", PrinterCard::kStatusReady), FindRow (snapshot, "Status").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Status"), "READY"));
            Assert::AreEqual (std::string ("off"), FindRow (snapshot, "Used").value);
            Assert::AreEqual (std::string ("1"),   FindRow (snapshot, "Slot").value);
        }


        //  The card's slot and latch, and its 6551's registers as the guest
        //  wrote them, decoded.
        TEST_METHOD (TheSerialCardPanelShowsTheCardAndIts6551)
        {
            TestMachine                   machine ("Apple2e", [] (MachineConfig & config) { EnableSlotDevice (config, kpszSuperSerialCardDevice); });
            const IDiagnosticsProvider  * card    = FindProvider (machine, "serial-card");
            DiagnosticsSnapshot           snapshot;



            Assert::IsNotNull (card, L"the card has a panel");

            machine.GetMemoryBus().WriteByte (0xC0AA, 0x0B);
            machine.GetMemoryBus().WriteByte (0xC0AB, 0x1E);

            snapshot = Snapshot (*card);

            Assert::AreEqual (std::string ("2"),    FindRow (snapshot, "Slot").value);
            Assert::AreEqual (std::string ("$0B"),  FindRow (snapshot, "Command").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Command"), "DTR"));
            Assert::AreEqual (std::string ("$1E"),  FindRow (snapshot, "Control").value);
            Assert::AreEqual (std::string ("9600"), FindRow (snapshot, "Baud").value);
            Assert::AreEqual (std::string ("8"),    FindRow (snapshot, "Word length").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Status"), "TX EMPTY"));
            Assert::AreEqual (std::string ("on"),   FindRow (snapshot, "DTR").value);
        }


        //  The //c's two built-in ports, each 6551 a panel.
        TEST_METHOD (TheIIcHasAPanelForEachSerialPort)
        {
            TestMachine                   machine ("Apple2c");
            const IDiagnosticsProvider  * port1   = FindProvider (machine, "serial-port-1");
            const IDiagnosticsProvider  * port2   = FindProvider (machine, "serial-port-2");



            Assert::IsNotNull (port1, L"port 1");
            Assert::IsNotNull (port2, L"port 2");

            machine.GetMemoryBus().WriteByte (0xC09A, 0x0B);

            Assert::AreEqual (std::string ("$0B"), FindRow (Snapshot (*port1), "Command").value, L"port 1 is $C098");
            Assert::AreEqual (std::string ("$00"), FindRow (Snapshot (*port2), "Command").value, L"port 2 is not");
            Assert::AreEqual (std::string ("none"), FindRow (Snapshot (*port1), "Attachment").value, L"nothing on a //c port as shipped");
        }


        //  Each card's bus registers at its slot's addresses, with the panel's
        //  labels for them, and the //c's two ports at theirs.
        TEST_METHOD (TheCardsDescribeTheirRegistersForTips)
        {
            TestMachine         cards ("Apple2e", [] (MachineConfig & config)
            {
                EnableSlotDevice (config, kpszSuperSerialCardDevice);
                EnableSlotDevice (config, "uthernet2");
            });
            TestMachine         iic   ("Apple2c");
            MachineDebugTarget  cardTarget (cards);
            MachineDebugTarget  iicTarget  (iic);
            DebugMachineInfo    cardInfo   = cardTarget.GetMachineInfo();
            DebugMachineInfo    iicInfo    = iicTarget.GetMachineInfo();



            Assert::AreEqual (std::string ("Super Serial Card, slot 2: SW1, the first block of DIP switches"), Describe (cardInfo, 0xC0A1));
            Assert::AreEqual (std::string ("Super Serial Card, slot 2: 6551 status register; a write resets the chip"), Describe (cardInfo, 0xC0A9));
            Assert::AreEqual (std::string ("Uthernet II, slot 3: W5100 mode register"), Describe (cardInfo, 0xC0B4));
            Assert::AreEqual (std::string(), Describe (cardInfo, 0xC0A4), L"no register there");
            Assert::AreEqual (std::string ("Serial port 1: 6551 data register; read the received byte, write a byte to send"), Describe (iicInfo, 0xC098));
            Assert::AreEqual (std::string ("Serial port 2: 6551 control register: baud rate, word length and stop bits"), Describe (iicInfo, 0xC0AB));
        }


        static std::string Describe (const DebugMachineInfo & info, Word address)
        {
            for (const DiagnosticsIoRegister & each : info.ioRegisters)
            {
                if (each.address == address)
                {
                    return each.description;
                }
            }

            return {};
        }


        //  The W5100's mode, addresses and sockets, as the guest set them.
        TEST_METHOD (TheUthernetPanelShowsTheChipAndItsSockets)
        {
            TestMachine                   machine ("Apple2e", [] (MachineConfig & config) { EnableSlotDevice (config, "uthernet2"); });
            const IDiagnosticsProvider  * card    = FindProvider (machine, "uthernet");
            MemoryBus                   & bus     = machine.GetMemoryBus();
            DiagnosticsSnapshot           snapshot;
            Byte                          ip[]    = { 192, 168, 1, 10 };



            Assert::IsNotNull (card, L"the card has a panel");

            bus.WriteByte (0xC0B4, 0x03);
            bus.WriteByte (0xC0B5, 0x00);
            bus.WriteByte (0xC0B6, 0x0F);

            for (Byte each : ip)
            {
                bus.WriteByte (0xC0B7, each);
            }

            bus.WriteByte (0xC0B5, 0x05);
            bus.WriteByte (0xC0B6, 0x00);
            bus.WriteByte (0xC0B7, 0x02);
            bus.WriteByte (0xC0B6, 0x04);
            bus.WriteByte (0xC0B7, 0x19);
            bus.WriteByte (0xC0B7, 0x66);
            bus.WriteByte (0xC0B6, 0x01);
            bus.WriteByte (0xC0B7, 0x01);

            snapshot = Snapshot (*card);

            Assert::AreEqual (std::string ("3"),            FindRow (snapshot, "Slot").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Mode"), "IND"));
            Assert::AreEqual (std::string ("192.168.1.10"), FindRow (snapshot, "IP address").value);
            Assert::AreEqual (std::string ("UDP, port 6502"), FindRow (snapshot, "Socket 1").value);
            Assert::AreEqual (std::string ("closed"),       FindRow (snapshot, "Socket 0").value);
            Assert::AreEqual (std::string ("on"),           FindRow (snapshot, "In use").value);
        }


        TEST_METHOD (TheClockPanelShowsCyclesAndSpeed)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  before;
            DiagnosticsSnapshot  after;



            before = Snapshot (machine);
            (void) machine.RunCycles (1000);
            machine.SetSpeedMode (SpeedMode::Double);
            after  = Snapshot (machine);

            Assert::AreEqual    (std::string ("authentic"), FindRow (before, "Speed").value);
            Assert::AreEqual    (std::string ("double"),    FindRow (after,  "Speed").value);
            Assert::AreNotEqual (FindRow (before, "Cycles").value, FindRow (after, "Cycles").value);
            (void) FindRow (after, "Scanline");
        }


        //  Only the current machine's devices are listed, the clock first.
        TEST_METHOD (TheProvidersAreTheMachinesOwn)
        {
            TestMachine               iie   ("Apple2e",    TestMachine::Slots::AsShipped);
            TestMachine               plus  ("Apple2Plus", TestMachine::Slots::DiskOnly);
            std::vector<std::string>  iieIds  = GetIds (iie);
            std::vector<std::string>  plusIds = GetIds (plus);



            Assert::AreEqual (std::string ("clock"), iieIds.front());

            for (const char * id : { "keyboard", "video", "mmu", "disk", "mockingboard" })
            {
                Assert::IsTrue (Contains (iieIds, id), std::wstring (id, id + strlen (id)).c_str());
            }

            for (const char * id : { "clock", "keyboard", "video", "disk" })
            {
                Assert::IsTrue (Contains (plusIds, id), std::wstring (id, id + strlen (id)).c_str());
            }

            Assert::IsFalse (Contains (plusIds, "mmu"),          L"a ][+ has no MMU");
            Assert::IsFalse (Contains (plusIds, "mockingboard"), L"nor a card in an empty slot");
            Assert::IsTrue  (Contains (iieIds,  "cassette"),     L"the //e has cassette jacks");
            Assert::IsTrue  (Contains (plusIds, "cassette"),     L"and the ][+");
        }


        //  The //c has no cassette port, so no Cassette panel.
        TEST_METHOD (TheIIcHasNoCassettePanel)
        {
            TestMachine  iic ("Apple2c", TestMachine::Slots::AsShipped);



            Assert::IsFalse (Contains (GetIds (iic), "cassette"));
        }


        //  The Cassette panel with no tape, then after the guest toggles the
        //  output twice: the transport, the output's level and the toggles.
        TEST_METHOD (TheCassettePanelShowsTheTransportAndTheOutput)
        {
            TestMachine                   machine  ("Apple2e", TestMachine::Slots::Empty);
            const IDiagnosticsProvider  * cassette = nullptr;
            DiagnosticsSnapshot           before;
            DiagnosticsSnapshot           after;



            machine.PowerCycle();

            for (const IDiagnosticsProvider * provider : machine.GetDiagnosticsProviders())
            {
                cassette = (provider->GetDiagnosticsId() == "cassette") ? provider : cassette;
            }

            if (cassette == nullptr)
            {
                Assert::Fail (L"the //e lists a Cassette panel");
                return;
            }

            before = Snapshot (*cassette);
            (void) machine.GetMemoryBus().ReadByte (0xC020);
            (void) machine.GetMemoryBus().ReadByte (0xC020);
            after  = Snapshot (*cassette);

            Assert::AreEqual (std::string ("No tape"), FindRow (before, "Transport").value);
            Assert::AreEqual (std::string ("none"),    FindRow (before, "Last access").value);
            Assert::AreEqual (std::string ("0"),       FindRow (before, "Output toggles").value);
            Assert::AreEqual (std::string ("2"),       FindRow (after,  "Output toggles").value);
            Assert::AreEqual (std::string ("off"),     FindRow (after,  "Output level").value, L"two toggles, back where it began");
        }


        TEST_METHOD (CassetteSecondsAreMinutesSecondsAndTenths)
        {
            Assert::AreEqual (std::string ("0:00.0"), CassetteDiagnostics::FormatSeconds (0.0));
            Assert::AreEqual (std::string ("2:05.3"), CassetteDiagnostics::FormatSeconds (125.25));
            Assert::AreEqual (std::string ("0:59.9"), CassetteDiagnostics::FormatSeconds (59.94));
        }
    };
}
