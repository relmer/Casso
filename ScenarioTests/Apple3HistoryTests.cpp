#include "Pch.h"

#include "EmuTests/GuestSession.h"
#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/TextEncoding.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Debugger/Reverse/StateDiff.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/Apple3/Apple3TextCells.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3HistoryTests
//
//  A run of Business BASIC on the Apple /// with history on, then a replay of the
//  whole history checked against every keyframe's checksum, and a seek back
//  to the live end, after which the machine is as it was live. The run
//  types a program, runs it, catalogs the disk and prints the time, so the
//  keyboard, the bank switching of the interpreter, the disk and the clock
//  are in history.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3HistoryTests)
{
public:

    static constexpr uint64_t  kTicksPerSecond  = 2040968;
    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint64_t  kSeconds         = 20;
    static constexpr uint64_t  kKeyframeFrames  = 2;
    static constexpr int       kSecondsToPrompt = 40;


    TEST_METHOD (BusinessBasic_ReplaysToEveryKeyframe_AndBackToLive)
    {
        TestMachine        machine ("Apple3");
        ReverseController  controller (machine);
        NeverStops         neverStops;
        ReverseSettings    settings;
        ReverseResult      result;
        std::vector<Byte>  live;
        HRESULT            hr        = S_OK;
        size_t             keyframes = 0;
        uint64_t           oldest    = 0;
        uint64_t           frames    = kSeconds * kFramesPerSecond;
        std::string        typed     = "10 FOR I = 1 TO 200 : PRINT I * 7 : NEXT\rRUN\rCATALOG\rPRINT TIME$\r";
        size_t             next      = 0;



        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilPrompt (machine), L"Business BASIC is at its prompt");

        settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles * kKeyframeFrames;

        hr = controller.Start (settings);
        AssertSucceeded (hr, L"Start");

        for (uint64_t frame = 0; frame < frames; frame++)
        {
            Apple3Keyboard  & keyboard = machine.GetApple3()->GetKeyboard();

            if (next < typed.size() && keyboard.IsStrobeClear() && frame % 3 == 0)
            {
                keyboard.PressKey ((Byte) typed[next++]);
            }

            machine.GetHostInput().Sample();
            machine.GetRunner().RunCycles (KeyframeSettings::kFrameCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        live      = Save (machine);
        keyframes = controller.GetKeyframes().GetArena().GetCount();

        oldest    = controller.GetSeeker().GetOldestPosition();

        Assert::IsTrue (keyframes > kSeconds, L"the run spans many keyframes");

        hr = controller.GetSeeker().ReverseContinue (neverStops, result);
        AssertSucceeded (hr, L"ReverseContinue over the whole history");

        Assert::IsTrue (result.outcome != ReverseOutcome::HistoryCut, TextEncoding::Utf8ToWide ("the replay matched every keyframe: " + controller.GetSeeker().GetLastDivergence()).c_str());
        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"nothing stopped the replay, so the machine is at the start");
        Assert::AreEqual<uint64_t> (oldest, machine.GetRunner().GetPosition(), L"at the oldest position");

        hr = controller.GetSeeker().SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        CheckState (live, Save (machine));
    }


    //  A machine loaded from a state saved while the disk is being read runs
    //  on as the one it was saved from.
    TEST_METHOD (ALoadMidDiskAccess_RunsAsTheSavedMachine)
    {
        TestMachine                   running ("Apple3");
        TestMachine                   loaded  ("Apple3");
        StateWriter                   writer;
        std::vector<IMachineState *>  runningParts;
        std::vector<IMachineState *>  loadedParts;
        HRESULT                       hr = S_OK;



        running.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        loaded.GetDiskStore().SetFlushSink  ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        MountBusinessBasic (running);
        MountBusinessBasic (loaded);
        Assert::IsTrue (RunUntilPrompt (running));

        for (char c : std::string ("CATALOG\r"))
        {
            running.GetApple3()->GetKeyboard().PressKey ((Byte) c);
            running.GetRunner().RunCycles (kTicksPerSecond / 20);
        }

        running.GetRunner().RunCycles (kTicksPerSecond / 5);

        hr = running.GetStateStream().SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        StateReader  reader (writer.GetBytes());

        hr = loaded.GetStateStream().LoadStateOverMountedMedia (reader);
        AssertSucceeded (hr, L"LoadState");

        for (int step = 0; step < 200; step++)
        {
            running.GetRunner().RunCycles (kTicksPerSecond / 100);
            loaded.GetRunner().RunCycles  (kTicksPerSecond / 100);

            runningParts.clear();
            loadedParts.clear();
            running.GetApple3()->GetStateParts (runningParts);
            loaded.GetApple3()->GetStateParts  (loadedParts);
            runningParts.insert (runningParts.begin(), running.GetCpu());
            loadedParts.insert  (loadedParts.begin(),  loaded.GetCpu());

            for (size_t i = 0; i < runningParts.size(); i++)
            {
                Assert::IsTrue (Save (*runningParts[i]) == Save (*loadedParts[i]), std::format (L"part {} matches after {} steps", i, step + 1).c_str());
            }
        }
    }


    //  A state loaded back into the machine it came from runs to the same
    //  machine again, as a replay from a keyframe does.
    TEST_METHOD (ALoadIntoTheSameMachine_RunsToTheSameState)
    {
        TestMachine                   machine ("Apple3");
        StateWriter                   first;
        std::vector<Byte>             later;
        std::vector<Byte>             again;
        HRESULT                       hr = S_OK;



        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilPrompt (machine));

        for (char c : std::string ("CATALOG\r"))
        {
            machine.GetApple3()->GetKeyboard().PressKey ((Byte) c);
            machine.GetRunner().RunCycles (kTicksPerSecond / 20);
        }

        machine.GetRunner().RunCycles (kTicksPerSecond / 5);

        hr = machine.GetStateStream().SaveState (first);
        AssertSucceeded (hr, L"SaveState");

        machine.GetRunner().RunCycles (kTicksPerSecond * 2);
        later = SaveAll (machine);

        StateReader  reader (first.GetBytes());

        hr = machine.GetStateStream().LoadStateOverMountedMedia (reader);
        AssertSucceeded (hr, L"LoadState");

        machine.GetRunner().RunCycles (kTicksPerSecond * 2);
        again = SaveAll (machine);

        Assert::IsTrue (again == later, TextEncoding::Utf8ToWide (StateDiff::Describe (later, again)).c_str());
    }


    static std::vector<Byte> SaveAll (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = machine.GetStateStream().SaveState (writer);



        AssertSucceeded (hr, L"SaveState");
        return writer.GetBytes();
    }

    static std::vector<Byte> Save (IMachineState & part)
    {
        StateWriter  writer;
        HRESULT      hr = part.SaveState (writer);



        AssertSucceeded (hr, L"SaveState");
        return writer.GetBytes();
    }


private:

    class NeverStops : public IReverseStopTest
    {
    public:
        bool          ShouldStopBefore (MachineHost &, Word) override { return false; }
        bool          TakePendingStop  () override                    { return false; }
        IWatchSink *  GetWatchSink     () override                    { return nullptr; }
    };


    static void MountBusinessBasic (TestMachine & machine)
    {
        std::vector<Byte>  bytes = GuestSession::RequireRepoImage ("Disks/Apple/apple3-business-basic.dsk");
        HRESULT            hr    = machine.GetDiskStore().MountFromBytes (6, 0, "apple3-business-basic.dsk", DiskFormat::Dsk, bytes);



        AssertSucceeded (hr, L"the Business BASIC disk mounts");
        machine.GetStorage().ConnectDiskToController (6, 0);
    }


    //  The screen as text, 80 columns when the 80-column switch is set.
    static std::string GetScreenText (Apple3System & system)
    {
        bool         is80    = system.GetVideo().Is80Columns();
        int          columns = is80 ? Apple3TextCells::kColumns * 2 : Apple3TextCells::kColumns;
        std::string  text;



        for (int row = 0; row < Apple3TextCells::kRows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                Word  page = (!is80 || column % 2 == 0) ? Apple3TextCells::kPage1 : Apple3TextCells::kPage2;
                Word  cell = (Word) (Apple3TextCells::GetRowAddress (row, page) + (is80 ? column / 2 : column));
                Byte  c    = (Byte) (system.GetMmu().PeekSystem (cell) & 0x7F);



                text += (c >= 0x20 && c < 0x7F) ? (char) c : ' ';
            }

            text += '\n';
        }

        return text;
    }


    //  Runs until the startup program's last line is on the screen.
    static bool RunUntilPrompt (TestMachine & machine)
    {
        bool  isFound = false;



        for (int second = 0; second < kSecondsToPrompt && !isFound; second++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond);
            isFound = GetScreenText (*machine.GetApple3()).find ("BLOCKS FREE") != std::string::npos;
        }

        return isFound;
    }

    static std::vector<Byte> Save (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = machine.GetStateStream().SaveState (writer);



        AssertSucceeded (hr, L"SaveState");
        return writer.GetBytes();
    }


    static void CheckState (const std::vector<Byte> & live, const std::vector<Byte> & replayed)
    {
        size_t  size  = std::min (live.size(), replayed.size());
        size_t  first = 0;



        Assert::AreEqual (live.size(), replayed.size(), L"state size at the live end");

        while (first < size && live[first] == replayed[first])
        {
            first++;
        }

        Assert::AreEqual (size, first, std::format (L"the machine at the live end differs from live first at byte {}", first).c_str());
    }
};
