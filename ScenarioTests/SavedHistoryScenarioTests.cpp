#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Devices/Disk/DiskImage.h"
#include "Shell/MachineStateFile.h"
#include "Shell/SavedHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistoryScenarioTests
//
//  History saved in a state file on a //e booted from the DOS 3.3 master:
//  a session with a fork, saved and loaded into a fresh machine, switches
//  branches there as on the machine it was saved from; and a history
//  filled to the default budget is saved and loaded, with the times logged
//  (SC-007 allows a second for the load).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SavedHistoryScenarioTests)
{
public:

    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint64_t  kMostSeconds     = 8000;
    static constexpr double    kFullShare       = 0.9;
    static constexpr double    kMsPerSecond     = 1e3;


    static void RunSeconds (TestMachine & machine, uint64_t seconds)
    {
        uint64_t  frame = 0;



        for (frame = 0; frame < seconds * kFramesPerSecond; frame++)
        {
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }
    }


    static uint64_t Checksum (MachineHost & machine)
    {
        StateWriter  writer;
        HRESULT      hr = machine.SaveState (writer);



        AssertSucceeded (hr, L"SaveState");

        return KeyframeStore::ComputeChecksum (writer.GetBytes().data(), writer.GetBytes().size());
    }


    static double ElapsedMs (const LARGE_INTEGER & start)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  now       = {};



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&now);

        return static_cast<double> (now.QuadPart - start.QuadPart) * kMsPerSecond / static_cast<double> (frequency.QuadPart);
    }


    //  The machine and its history as a state file's bytes.
    static std::vector<Byte> Save (TestMachine & machine, ReverseHost & host)
    {
        SavedHistoryContents  contents;
        std::vector<Byte>     section;
        std::vector<Byte>     bytes;
        MachineStateError     error;
        HRESULT               hr       = S_OK;



        hr = machine.ComputeStateFingerprint (contents.fingerprint);
        AssertSucceeded (hr, L"ComputeStateFingerprint");

        hr = host.GetController().SaveHistory (contents.keyframes);
        AssertSucceeded (hr, L"SaveHistory");

        for (int slot = 0; slot < DiskImageStore::kSlotCount; slot++)
        {
            for (int drive = 0; drive < DiskImageStore::kDriveCount; drive++)
            {
                if (machine.GetDiskStore().GetMediaId (slot, drive) != 0)
                {
                    contents.media.push_back (SavedBayMedium { slot, drive, machine.GetDiskStore().GetMediaId (slot, drive) });
                }
            }
        }

        contents.resumePosition = machine.GetPosition();

        hr = SavedHistory::Build (contents, section);
        AssertSucceeded (hr, L"SavedHistory::Build");

        hr = MachineStateFile::Build (machine, std::vector<Byte>(), section, bytes, error);
        AssertSucceeded (hr, L"MachineStateFile::Build");

        return bytes;
    }


    //  A state file's bytes loaded into a machine, its disks given back the
    //  ids its history uses for them, and recording started from it, as the
    //  shell does.
    static void Load (TestMachine & machine, ReverseHost & host, const std::vector<Byte> & bytes)
    {
        MachineStateContents  contents;
        SavedHistoryContents  history;
        MachineStateError     error;
        uint64_t              fingerprint = 0;
        HRESULT               hr          = S_OK;



        hr = MachineStateFile::Parse (bytes, contents, error);
        AssertSucceeded (hr, L"MachineStateFile::Parse");

        hr = MachineStateFile::Apply (machine, contents, error);
        AssertSucceeded (hr, L"MachineStateFile::Apply");

        hr = SavedHistory::Parse (contents.historySection.data(), contents.historySection.size(), history);
        AssertSucceeded (hr, L"SavedHistory::Parse");

        hr = machine.ComputeStateFingerprint (fingerprint);
        AssertSucceeded (hr, L"ComputeStateFingerprint");
        Assert::AreEqual<uint64_t> (history.fingerprint, fingerprint, L"the same build saved it");

        for (const SavedBayMedium & medium : history.media)
        {
            machine.GetDiskStore().GetImage (medium.slot, medium.drive)->SetImageId (medium.mediaId);
        }

        hr = host.StartRecordingFromSaved (ReverseSettings(), history.keyframes);
        AssertSucceeded (hr, L"StartRecordingFromSaved");
    }


    static void Execute (ReverseHost & host, ReverseCommand command, uint64_t argument)
    {
        ReverseResult  result;
        HRESULT        hr = host.Execute (command, argument, nullptr, result);



        AssertSucceeded (hr, L"Execute");
    }


    TEST_METHOD (AForkedDosSessionSwitchesBranchesInAFreshMachine)
    {
        TestMachine                  source     ("Apple2eEnhanced");
        TestMachine                  target     ("Apple2eEnhanced");
        ReverseHost                  sourceHost (source);
        ReverseHost                  targetHost (target);
        std::vector<Byte>            bytes;
        std::vector<KeyframeBranch>  branches;
        uint64_t                     forkAt     = 0;
        HRESULT                      hr         = S_OK;



        source.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        target.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (source, GuestSession::RequireDos33Master());

        hr = sourceHost.StartRecording (ReverseSettings());
        AssertSucceeded (hr, L"StartRecording");

        KeystrokeInjector::InjectLine (source, "10 PRINT \"FIRST\" : GOTO 10");
        KeystrokeInjector::InjectLine (source, "RUN");
        RunSeconds (source, 3);

        forkAt = (sourceHost.GetController().GetOldestPosition() + source.GetPosition()) / 2;

        Execute (sourceHost, ReverseCommand::Seek, forkAt);

        hr = sourceHost.Fork();
        AssertSucceeded (hr, L"Fork");

        KeystrokeInjector::InjectLine (source, "PRINT \"SECOND\"");
        RunSeconds (source, 2);

        bytes = Save (source, sourceHost);

        Load (target, targetHost, bytes);

        Assert::AreEqual<uint64_t> (source.GetPosition(), target.GetPosition(), L"at the same live end");
        Assert::AreEqual<uint64_t> (Checksum (source),    Checksum (target),    L"with the same machine");

        sourceHost.GetController().GetKeyframes().GetBranches (branches);
        Assert::AreEqual<size_t> (1, branches.size(), L"one branch");

        Execute (sourceHost, ReverseCommand::SwitchBranch, branches[0].id);
        Execute (targetHost, ReverseCommand::SwitchBranch, branches[0].id);

        Assert::AreEqual<uint64_t> (source.GetPosition(), target.GetPosition(), L"both at the branch's live end");
        Assert::AreEqual<uint64_t> (Checksum (source),    Checksum (target),    L"with the same machine");

        sourceHost.GetController().GetKeyframes().GetBranches (branches);

        Execute (sourceHost, ReverseCommand::SwitchBranch, branches[0].id);
        Execute (targetHost, ReverseCommand::SwitchBranch, branches[0].id);

        Assert::AreEqual<uint64_t> (Checksum (source), Checksum (target), L"and back on the path that was active");
    }


    //  A //e drawing hi-res lines at random until its history fills most of the
    //  default budget, saved and loaded, each timed.
    TEST_METHOD (AFullHistorySavesAndLoads)
    {
        TestMachine        source     ("Apple2eEnhanced");
        TestMachine        target     ("Apple2eEnhanced");
        ReverseHost        sourceHost (source);
        ReverseHost        targetHost (target);
        std::vector<Byte>  bytes;
        size_t             full       = static_cast<size_t> (KeyframeSettings::kDefaultBudgetBytes * kFullShare);
        uint64_t           seconds    = 0;
        double             saveMs     = 0;
        double             loadMs     = 0;
        LARGE_INTEGER      start      = {};
        HRESULT            hr         = S_OK;



        source.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        target.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (source, GuestSession::RequireDos33Master());

        hr = sourceHost.StartRecording (ReverseSettings());
        AssertSucceeded (hr, L"StartRecording");

        KeystrokeInjector::InjectLine (source, "10 HGR : HCOLOR= 3");
        KeystrokeInjector::InjectLine (source, "20 HCOLOR= RND (1) * 8 : Y = RND (1) * 159 : HPLOT 0,Y TO 279,Y : GOTO 20");
        KeystrokeInjector::InjectLine (source, "RUN");

        while (sourceHost.GetController().GetKeyframes().GetByteCount() < full && seconds < kMostSeconds)
        {
            RunSeconds (source, 10);
            seconds += 10;
        }

        QueryPerformanceCounter (&start);
        bytes  = Save (source, sourceHost);
        saveMs = ElapsedMs (start);

        QueryPerformanceCounter (&start);
        Load (target, targetHost, bytes);
        loadMs = ElapsedMs (start);

        Assert::AreEqual<uint64_t> (Checksum (source), Checksum (target), L"the loaded machine is the saved one");

        Logger::WriteMessage (std::format ("saved history: {} s of recording, {} keyframes in {} bytes; file {} bytes; save {:.0f} ms, load {:.0f} ms\n",
                                           seconds,
                                           sourceHost.GetController().GetKeyframes().GetCount(),
                                           sourceHost.GetController().GetKeyframes().GetByteCount(),
                                           bytes.size(),
                                           saveMs,
                                           loadMs).c_str());
    }
};
