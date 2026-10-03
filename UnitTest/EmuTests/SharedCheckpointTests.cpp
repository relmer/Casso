#include "Pch.h"

#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Core/IMachineState.h"
#include "Core/StateHash.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/UndoRing.h"
#include "Devices/Disk/DiskImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint32_t  s_kOuterTag      = IMachineState::MakeTag ('O', 'U', 'T', 'R');
static constexpr uint32_t  s_kInnerTag      = IMachineState::MakeTag ('I', 'N', 'N', 'R');
static constexpr uint64_t  s_kSharedFrames  = 40;
static constexpr size_t    s_kSegmentBytes  = 1000;
static constexpr size_t    s_kOwnBytes      = 300;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedCheckpointTests
//
//  Ring checkpoints taken by a sharing StateWriter keep a disk track by
//  reference until the track changes. Each test holds a sharing save to the
//  plain save of the same machine: flattened, the two must be the same
//  bytes, before and after the disk is written and after a load, and only a
//  track that changed may get a new buffer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SharedCheckpointTests)
{
public:

    TEST_METHOD (ASharingWriterFlattensToThePlainBlob)
    {
        std::shared_ptr<const std::vector<Byte>>  first  = MakeBuffer (s_kSegmentBytes, 3);
        std::shared_ptr<const std::vector<Byte>>  second = MakeBuffer (s_kSegmentBytes + 7, 5);
        StateWriter                               plain;
        StateWriter                               sharing;
        std::vector<Byte>                         flat;



        sharing.SetSharing (true);

        WriteNested (plain,   first, second);
        WriteNested (sharing, first, second);

        Assert::IsFalse (sharing.HasOpenSection(), L"every section closed");
        Assert::IsTrue  (sharing.GetBytes().size() < plain.GetBytes().size(), L"the shared runs were not copied");

        StateWriter::Flatten (sharing.GetBytes(), sharing.TakeSegments(), flat);

        Assert::IsTrue (flat == plain.GetBytes(), L"section sizes count the shared runs, and the runs land where they were written");
    }


    TEST_METHOD (ASharedMachineSaveMatchesThePlainOneAsTheDiskChanges)
    {
        TestMachine                machine ("Apple2e");
        DiskImage                * disk    = nullptr;
        std::vector<StateSegment>  before;
        std::vector<StateSegment>  after;
        std::vector<uint64_t>      generations;
        size_t                     changed = 0;
        size_t                     track   = 0;
        bool                       isSame  = false;



        ReverseSessionRig::Prepare (machine);

        disk = machine.GetDiskStore().GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive);
        Assert::IsNotNull (disk, L"the rig's disk");

        machine.RunCycles (KeyframeSettings::kFrameCycles);

        before = SaveSharedAndCompare (machine, L"before the disk is written");
        Assert::AreEqual<size_t> (static_cast<size_t> (disk->GetTrackCount()), before.size(), L"one shared run per track");

        for (track = 0; track < before.size(); track++)
        {
            generations.push_back (disk->GetTrackGeneration (static_cast<int> (track)));
        }

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kSharedFrames);

        after = SaveSharedAndCompare (machine, L"after the guest wrote the disk");

        for (track = 0; track < after.size(); track++)
        {
            isSame   = generations[track] == disk->GetTrackGeneration (static_cast<int> (track));
            changed += isSame ? 0 : 1;

            Assert::AreEqual (isSame, before[track].bytes == after[track].bytes, L"a track keeps its buffer exactly while it is unchanged");
        }

        Assert::IsTrue (changed > 0,            L"the guest wrote at least one track");
        Assert::IsTrue (changed < after.size(), L"and left others alone to share");
    }


    TEST_METHOD (ASharedSaveAfterALoadMatchesThePlainOne)
    {
        TestMachine        machine ("Apple2e");
        std::vector<Byte>  earlier;
        HRESULT            hr      = S_OK;



        ReverseSessionRig::Prepare (machine);
        machine.RunCycles (KeyframeSettings::kFrameCycles);

        earlier = ReverseSessionRig::Save (machine);

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kSharedFrames);
        SaveSharedAndCompare (machine, L"before the load");

        hr = Load (machine, earlier);
        AssertSucceeded (hr, L"LoadState");

        SaveSharedAndCompare (machine, L"after loading the earlier state");
        Assert::IsTrue (ReverseSessionRig::Save (machine) == earlier, L"the load took");
    }


    TEST_METHOD (TheRingCountsABufferSharedByCheckpointsOnce)
    {
        UndoRing                                  ring;
        UndoRingSettings                          settings;
        std::shared_ptr<const std::vector<Byte>>  shared = MakeBuffer (s_kSegmentBytes, 9);
        size_t                                    empty  = 0;
        HRESULT                                   hr     = S_OK;



        settings.budgetBytes = 0;       // the fewest checkpoints, so adding them does not grow the records

        AssertSucceeded (ring.Configure (settings, KeyframeSettings::kFrameCycles), L"Configure");
        empty = ring.GetByteCount();

        hr = ring.AddCheckpoint (10, 100, 0, std::vector<Byte> (s_kOwnBytes), { StateSegment { 0, shared } });
        AssertSucceeded (hr, L"first AddCheckpoint");

        hr = ring.AddCheckpoint (20, 200, 0, std::vector<Byte> (s_kOwnBytes), { StateSegment { 0, shared } });
        AssertSucceeded (hr, L"second AddCheckpoint");

        Assert::AreEqual (empty + 2 * s_kOwnBytes + s_kSegmentBytes, ring.GetByteCount(), L"own bytes twice, the shared run once");
    }


    TEST_METHOD (StateHashCoversEveryByteAndChains)
    {
        std::vector<Byte>  bytes (s_kOwnBytes + 5, 0x5A);
        uint64_t           whole = StateHash::Hash (bytes.data(), bytes.size());
        size_t             i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] ^= 1;
            Assert::AreNotEqual (whole, StateHash::Hash (bytes.data(), bytes.size()), L"a bit anywhere, the tail included, changes it");
            bytes[i] ^= 1;
        }

        Assert::AreNotEqual (StateHash::Hash (bytes.data(), bytes.size()),
                             StateHash::Hash (bytes.data(), bytes.size(), whole),
                             L"the seed carries a previous run in");
    }

private:

    static std::shared_ptr<const std::vector<Byte>> MakeBuffer (size_t size, Byte step)
    {
        std::vector<Byte>  bytes (size);
        size_t             i     = 0;



        for (i = 0; i < size; i++)
        {
            bytes[i] = static_cast<Byte> (i * step);
        }

        return std::make_shared<const std::vector<Byte>> (std::move (bytes));
    }


    static void WriteNested (
        StateWriter                                     & writer,
        const std::shared_ptr<const std::vector<Byte>>  & first,
        const std::shared_ptr<const std::vector<Byte>>  & second)
    {
        HRESULT  hr = S_OK;



        writer.BeginSection (s_kOuterTag, 1);
        writer.WriteByte    (0x11);
        writer.WriteShared  (first);
        writer.BeginSection (s_kInnerTag, 2);
        writer.WriteUInt32  (0x22334455);
        writer.WriteShared  (second);
        writer.WriteShared  (first);

        hr = writer.EndSection();
        AssertSucceeded (hr, L"inner EndSection");

        writer.WriteWord (0x6677);

        hr = writer.EndSection();
        AssertSucceeded (hr, L"outer EndSection");
    }


    static HRESULT Load (MachineHost & machine, const std::vector<Byte> & state)
    {
        StateReader  reader (state);



        return machine.LoadState (reader);
    }


    static std::vector<StateSegment> SaveSharedAndCompare (MachineHost & machine, const wchar_t * when)
    {
        StateWriter                writer;
        std::vector<StateSegment>  segments;
        std::vector<Byte>          flat;
        HRESULT                    hr       = S_OK;



        writer.SetSharing (true);

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"sharing SaveState");

        segments = writer.TakeSegments();

        StateWriter::Flatten (writer.GetBytes(), segments, flat);

        Assert::IsFalse (segments.empty(), when);
        Assert::IsTrue  (flat == ReverseSessionRig::Save (machine), when);

        return segments;
    }
};
