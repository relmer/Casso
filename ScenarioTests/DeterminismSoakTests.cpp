#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/MachineConfig.h"
#include "Core/StateWriter.h"
#include "Core/TextEncoding.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeterminismSoakTests
//
//  Long recorded runs of real software on every machine kind, then a replay
//  of the whole history checked against every keyframe's checksum. Each
//  machine boots the DOS 3.3 master and runs an Applesoft program that draws
//  on the hi-res page, clicks the speaker, times paddle 0, reads the keyboard,
//  sends a byte through a serial port with a loopback plug and reads it back,
//  and catalogs the disk now and then. Keys and paddle moves are fed between
//  frames as the UI thread feeds them, and the run takes a reset and a power
//  cycle, both recorded as the shell records them.
//
//  A reverse continue that nothing stops replays every stretch of history
//  back to its start, so a keyframe that a replay does not reach exactly
//  cuts history there and fails the case. A seek to the live end must then
//  give back the machine as it was live.
//
//  The cards case adds the Super Serial Card, its connector on a loopback
//  plug, and the Uthernet II, which the guest reads but never opens a socket
//  on; history is off while the card has one open.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeterminismSoakTests)
{
public:

    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint64_t  kSeconds         = 40;
    static constexpr uint64_t  kKeyframeFrames  = 2;
    static constexpr uint64_t  kKeyEvery        = 23;
    static constexpr uint64_t  kPaddleEvery     = 7;
    static constexpr uint64_t  kPaddleStep      = 37;


    TEST_METHOD (EveryMachineAsShipped)
    {
        std::vector<Byte>  master = GuestSession::RequireDos33Master();



        for (const char * machineId : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
        {
            TestMachine  machine (machineId);

            Soak (machine, machineId, master);
        }
    }


    TEST_METHOD (EveryMachineWithTheCards)
    {
        std::vector<Byte>  master = GuestSession::RequireDos33Master();



        for (const char * machineId : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
        {
            TestMachine  machine (machineId, TestMachine::Slots::AsShipped, [] (MachineConfig & config, MachineBuildServices &)
            {
                EnableCards (config);
            });

            Assert::IsNotNull (machine.GetRefs().superSerialCard, Widen (std::string (machineId) + ": the Super Serial Card is in").c_str());
            Assert::IsNotNull (machine.GetRefs().uthernet,        Widen (std::string (machineId) + ": the Uthernet II is in").c_str());

            Soak (machine, std::string (machineId) + " with the cards", master);
        }
    }


private:

    class NeverStops : public IReverseStopTest
    {
    public:
        bool          ShouldStopBefore (MachineHost &, Word) override { return false; }
        bool          TakePendingStop  () override                    { return false; }
        IWatchSink *  GetWatchSink     () override                    { return nullptr; }
    };


    static void Soak (TestMachine & machine, const std::string & label, const std::vector<Byte> & master)
    {
        ReverseController  controller (machine);
        NeverStops         neverStops;
        ReverseSettings    settings;
        ReverseResult      result;
        std::wstring       name       = Widen (label);
        std::vector<Byte>  live;
        HRESULT            hr         = S_OK;
        size_t             keyframes  = 0;
        uint64_t           oldest     = 0;
        uint64_t           frame      = 0;
        uint64_t           frames     = kSeconds * kFramesPerSecond;



        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (machine, master);
        EnterProgram (machine);

        settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles * kKeyframeFrames;

        hr = controller.Start (settings);
        AssertSucceeded (hr, (name + L": Start").c_str());

        for (frame = 0; frame < frames; frame++)
        {
            if (frame == frames / 3)
            {
                machine.RecordInput (InputKind::Reset, 0, 0, {});
                machine.SoftReset();
                TypeLine (machine, "RUN");
            }

            if (frame == (frames * 2) / 3)
            {
                machine.RecordInput (InputKind::PowerCycle, 0, 0, {});
                machine.PowerCycle();
            }

            Feed (machine, frame);
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, (name + L": WaitForPending").c_str());

        live      = Save (machine);
        keyframes = controller.GetKeyframes().GetCount();
        oldest    = controller.GetOldestPosition();

        Assert::IsTrue (keyframes > kSeconds, (name + L": the run spans many keyframes").c_str());

        hr = controller.ReverseContinue (neverStops, result);
        AssertSucceeded (hr, (name + L": ReverseContinue over the whole history").c_str());

        Assert::IsTrue (result.outcome != ReverseOutcome::HistoryCut, (name + L": the replay matched every keyframe").c_str());
        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, (name + L": nothing stopped it, so it reached the start").c_str());
        Assert::AreEqual<uint64_t> (oldest, machine.GetPosition(), (name + L": at the oldest position").c_str());
        Assert::AreEqual<size_t>   (keyframes, controller.GetKeyframes().GetCount(), (name + L": no keyframe was dropped").c_str());

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, (name + L": SeekToCycle live").c_str());

        CheckState (live, Save (machine), name);
        Assert::IsFalse (controller.IsInHistory(), (name + L": live again").c_str());

        Logger::WriteMessage (std::format ("{}: {} keyframes, {} journal records, all matched\n",
                                           label,
                                           keyframes,
                                           machine.GetInputJournal().GetEndIndex() - machine.GetInputJournal().GetBeginIndex()).c_str());
    }


    //  The serial port's data register, $C0A8 on a card in slot 2 and $C098
    //  for the //c's first port; the status, command and control registers
    //  follow it.
    static void EnterProgram (TestMachine & machine)
    {
        int          data       = (machine.GetRefs().superSerialCard != nullptr || machine.GetRefs().serialChips.empty()) ? 0xC0A8 : 0xC098;
        bool         isUthernet = machine.GetRefs().uthernet != nullptr;
        std::string  serial;



        serial = std::format ("POKE {},11 : POKE {},30 : POKE {},K : S = PEEK ({}) : D = PEEK ({})", data + 2, data + 3, data, data + 1, data);

        KeystrokeInjector::InjectLine (machine, "10 HGR : HCOLOR= 3 : K = 0");
        KeystrokeInjector::InjectLine (machine, "20 HPLOT RND (1) * 279, RND (1) * 159 : X = PEEK (-16336) : P = PDL (0)");
        KeystrokeInjector::InjectLine (machine, "30 K = PEEK (-16384) : IF K > 127 THEN POKE -16368,0 : K = K - 128");
        KeystrokeInjector::InjectLine (machine, "40 " + serial);
        KeystrokeInjector::InjectLine (machine, isUthernet ? "50 U = PEEK (49332)" : "50 U = 0");
        KeystrokeInjector::InjectLine (machine, "60 IF RND (1) < 0.002 THEN PRINT CHR$ (4);\"CATALOG\"");
        KeystrokeInjector::InjectLine (machine, "70 GOTO 20");
        KeystrokeInjector::InjectLine (machine, "RUN");
    }


    //  Typed while history records, so each key is pressed as the UI thread
    //  presses one and the machine runs through MachineHost, which counts
    //  positions; KeystrokeInjector steps the processor directly, which
    //  history cannot replay.
    static void TypeLine (TestMachine & machine, const std::string & text)
    {
        constexpr uint64_t  kStepCycles = 1000;
        constexpr int       kMaxSteps   = 3000;
        std::string         line        = text + "\r";
        int                 steps       = 0;



        for (char ch : line)
        {
            machine.GetRefs().keyboard->PressKey (static_cast<Byte> (ch));

            for (steps = 0; steps < kMaxSteps && !machine.GetRefs().keyboard->IsStrobeClear(); steps++)
            {
                machine.SampleHostInputs();
                machine.RunCycles (kStepCycles);
            }

            Assert::IsTrue (machine.GetRefs().keyboard->IsStrobeClear(), L"the guest takes each key");
        }
    }


    //  A key every kKeyEvery frames, a paddle move every kPaddleEvery, written
    //  as the UI thread writes them.
    static void Feed (TestMachine & machine, uint64_t frame)
    {
        Byte  position = static_cast<Byte> (frame * kPaddleStep);



        if (frame % kKeyEvery == kKeyEvery - 1)
        {
            machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('A' + (frame / kKeyEvery) % 26));
        }

        if (frame % kPaddleEvery != kPaddleEvery - 1)
        {
            return;
        }

        if (machine.GetRefs().iieSoftSwitches != nullptr)
        {
            machine.GetRefs().iieSoftSwitches->SetPaddle (0, position);
        }
        else if (machine.GetRefs().gamePort != nullptr)
        {
            machine.GetRefs().gamePort->SetPaddle (0, position);
        }
    }


    static void EnableCards (MachineConfig & config)
    {
        PortConfig  port;



        port.device = kpszSerialLoopback;

        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                slot.enabled        = true;
                slot.firmwarePath   = RequireFirmwarePath().string();
                slot.firmwareStatus = SlotFirmwareStatus::Present;
                slot.ports          = { port };
            }
            else if (slot.device == "uthernet2")
            {
                slot.enabled = true;
            }
        }
    }


    static std::filesystem::path RequireFirmwarePath()
    {
        char *                 root = nullptr;
        size_t                 len  = 0;
        std::filesystem::path  path;
        std::error_code        ec;



        if (_dupenv_s (&root, &len, "LOCALAPPDATA") == 0 && root != nullptr)
        {
            path = std::filesystem::path (root) / "Casso" / "Devices" / "SuperSerialCard" / "SSC.rom";
            free (root);
        }

        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec),
            L"Apple's Super Serial Card firmware is REQUIRED by this case and was not "
            L"found at %LOCALAPPDATA%\\Casso\\Devices\\SuperSerialCard\\SSC.rom. To get it, "
            L"turn the card on under Settings > Machine and accept the download");

        return path;
    }


    static std::vector<Byte> Save (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        return writer.GetBytes();
    }


    static void CheckState (const std::vector<Byte> & live, const std::vector<Byte> & replayed, const std::wstring & name)
    {
        size_t  size  = std::min (live.size(), replayed.size());
        size_t  first = 0;



        Assert::AreEqual (live.size(), replayed.size(), (name + L": state size at the live end").c_str());

        while (first < size && live[first] == replayed[first])
        {
            first++;
        }

        Assert::AreEqual (size, first, std::format (L"{}: the machine at the live end differs from live first at byte {}", name, first).c_str());
    }


    static std::wstring Widen (const std::string & text)
    {
        return TextEncoding::NarrowToWide (text);
    }
};
