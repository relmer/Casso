#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Core/StateWriter.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Shell/ScratchMachineRenderer.h"
#include "Video/MachineFrameRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int       s_kCostKeyframes   = 75;
static constexpr uint32_t  s_kCostGroup       = 30;
static constexpr int       s_kCostRepeats     = 40;
static constexpr int       s_kCostHandoffs    = 200;
static constexpr SIZE      s_kCostCell        = { 96, 66 };
static constexpr Word      s_kHiResPage       = 0x2000;
static constexpr Word      s_kHiResBytes      = 0x2000;
static constexpr Word      s_kHiResChanged    = 0x0400;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailCostTests
//
//  Not a pass or fail: what each stage of a history thumbnail costs, from
//  the keyframe to the picture, written to the test log. The machine is the
//  reverse rig's //e with a disk in the drive, showing a color hi-res screen
//  that changes between keyframes, so the snapshots, the differences and
//  the drawing are all of a realistic size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryThumbnailCostTests)
{
public:

    TEST_METHOD (EveryThumbnailStageIsMeasured)
    {
        TestMachine               machine ("Apple2e");
        KeyframeStore             store;
        KeyframeSettings          settings;
        ScratchMachineRenderer    scratch;
        ThreadPoolWorkQueue       queue;
        KeyframeUnpacker          unpacker;
        PackedKeyframe            packed;
        KeyframeCopy              copy     = KeyframeCopy::Gone;
        std::vector<Byte>         state;
        std::vector<Byte>         other;
        std::vector<uint32_t>     pixels;
        auto                      full     = std::make_shared<DxuiIconImage>();
        HistoryThumbnails::Image  base;
        DxuiIconImage             thumb;
        size_t                    oldWhole = 0;
        size_t                    oldDiff  = 0;
        size_t                    newDiff  = 0;
        size_t                    i        = 0;
        int                       width    = 0;
        int                       height   = 0;
        HRESULT                   hr       = S_OK;
        double                    frameMs  = 0.0;
        std::string               log;



        settings.wholeEvery   = s_kCostGroup;
        settings.longestGroup = s_kCostGroup;
        store.Configure (settings);

        ReverseSessionRig::Prepare (machine);
        ShowHiRes (machine);

        frameMs = TimeMs ([&] { machine.RunCycles (KeyframeSettings::kFrameCycles); });

        for (i = 0; i < (size_t) s_kCostKeyframes; i++)
        {
            machine.RunCycles (settings.intervalCycles);
            ScribbleHiRes (machine, i);

            hr = store.Capture (machine, machine.GetPosition());
            AssertSucceeded (hr, L"Capture");
        }

        //  Groups of s_kCostGroup: the oldest starts at 0, and the newest
        //  keyframe is a difference in the third.
        oldWhole = 0;
        oldDiff  = s_kCostGroup / 2;
        newDiff  = store.GetCount() - 1;

        Assert::IsTrue  (store.GetInfo (oldWhole).isWhole, L"the oldest keyframe is whole");
        Assert::IsFalse (store.GetInfo (oldDiff).isWhole,  L"one in the oldest group is a difference");
        Assert::IsFalse (store.GetInfo (newDiff).isWhole,  L"the newest is a difference in the newest group");

        log += std::format ("history thumbnail cost, {} build\n", IsDebugBuild() ? "Debug" : "Release");
        log += std::format ("  state {} bytes; whole packs to {}, a difference to {}\n",
                            store.GetInfo (0).stateBytes, store.GetInfo (oldWhole).storedBytes, store.GetInfo (oldDiff).storedBytes);
        log += std::format ("  emulating one frame (17,030 cycles)                {:8.3f} ms\n", frameMs);

        log += std::format ("  unpack, newest group (diff + XOR)                  {:8.3f} ms\n",
                            TimeMs ([&] { hr = store.Restore (newDiff, state); }));
        log += std::format ("  unpack, older group's whole snapshot               {:8.3f} ms\n",
                            TimeMs ([&] { hr = store.Restore (oldWhole, state); }));
        log += std::format ("  unpack, older group (whole + diff + XOR)           {:8.3f} ms\n",
                            TimeMs ([&] { hr = store.Restore (oldDiff, state); }));
        AssertSucceeded (hr, L"Restore");

        log += std::format ("  machine thread: copy packed, with the whole        {:8.4f} ms\n",
                            TimeMs ([&] { unpacker.Forget(); hr = store.CopyPacked (store.GetInfo (oldDiff).position, unpacker, packed, copy); }));
        AssertSucceeded (hr, L"CopyPacked");

        hr = unpacker.Unpack (packed, state);
        AssertSucceeded (hr, L"Unpack");

        log += std::format ("  machine thread: copy packed, unpacker holds whole  {:8.4f} ms\n",
                            TimeMs ([&] { hr = store.CopyPacked (store.GetInfo (oldDiff + 1).position, unpacker, packed, copy); }));
        AssertSucceeded (hr, L"CopyPacked");

        log += std::format ("  worker: unpack, same group (diff + XOR)            {:8.3f} ms\n",
                            TimeMs ([&] { hr = unpacker.Unpack (packed, state); }));
        AssertSucceeded (hr, L"Unpack");

        hr = store.CopyPacked (store.GetInfo (newDiff).position, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked");

        log += std::format ("  worker: unpack, other group (whole + diff + XOR)   {:8.3f} ms\n",
                            TimeMs ([&] { unpacker.Forget(); hr = unpacker.Unpack (packed, state); }));
        AssertSucceeded (hr, L"Unpack");

        other = state;

        log += std::format ("    of which the XOR alone                           {:8.3f} ms\n",
                            TimeMs ([&] { KeyframeStore::XorBytes (state.data(), other.data(), state.data(), state.size()); }));

        hr = unpacker.Unpack (packed, state);
        AssertSucceeded (hr, L"Unpack");

        scratch.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

        hr = scratch.Render (state, pixels, width, height);
        AssertSucceeded (hr, L"the first picture builds the scratch machine");

        log += std::format ("  scratch render: load state + draw 560x384          {:8.3f} ms\n",
                            TimeMs ([&] { hr = scratch.Render (state, pixels, width, height); }));
        log += std::format ("    of which drawing the frame alone                 {:8.3f} ms\n",
                            TimeMs ([&] { hr = MachineFrameRenderer::Render (machine, machine.GetBuilder(), true, pixels); }));
        AssertSucceeded (hr, L"Render");

        full->width      = width;
        full->height     = height;
        full->bgraPremul = pixels;

        log += std::format ("  base copy: 560x384 to 186x128                      {:8.3f} ms\n",
                            TimeMs ([&] { base = HistoryThumbnails::MakeBase (full); }));
        log += std::format ("  thumbnail: base to {}x{}                            {:8.3f} ms\n", s_kCostCell.cx, s_kCostCell.cy,
                            TimeMs ([&] { HistoryThumbnails::Shrink (*base, s_kCostCell.cx, s_kCostCell.cy, thumb); }));
        log += std::format ("  paint: hash one thumbnail's pixels (per cell/paint){:8.4f} ms\n",
                            TimeMs ([&] { thumb.bgraPremul[0]++; s_sink += Fnv (thumb.bgraPremul); }));

        hr = queue.Create (1);
        AssertSucceeded (hr, L"Create");

        log += std::format ("  handoff to the pool worker, submit to start        {:8.4f} ms\n", MeasureHandoffMs (queue));

        Logger::WriteMessage (log.c_str());
    }

private:

    static inline uint64_t  s_sink = 0;


    template <typename Fn>
    static double TimeMs (Fn fn)
    {
        LARGE_INTEGER  start = {};
        LARGE_INTEGER  end   = {};
        LARGE_INTEGER  freq  = {};
        int            i     = 0;



        fn();

        QueryPerformanceFrequency (&freq);
        QueryPerformanceCounter   (&start);

        for (i = 0; i < s_kCostRepeats; i++)
        {
            fn();
        }

        QueryPerformanceCounter (&end);

        return (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart / s_kCostRepeats;
    }


    static bool IsDebugBuild()
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
    }


    //  Graphics, hi-res, full screen, page 1.
    static void ShowHiRes (TestMachine & machine)
    {
        constexpr Word  kGraphics = 0xC050;
        constexpr Word  kFull     = 0xC052;
        constexpr Word  kPage1    = 0xC054;
        constexpr Word  kHiRes    = 0xC057;



        machine.GetMemoryBus().ReadByte (kGraphics);
        machine.GetMemoryBus().ReadByte (kFull);
        machine.GetMemoryBus().ReadByte (kPage1);
        machine.GetMemoryBus().ReadByte (kHiRes);

        ScribbleHiRes (machine, 0);

        Assert::IsTrue (machine.GetRefs().softSwitches->IsGraphicsMode(), L"the screen shows graphics");
        Assert::IsTrue (machine.GetRefs().softSwitches->IsHiresMode(),    L"in hi-res");
    }


    //  A pseudo-random stretch of the hi-res page, moved on each keyframe.
    static void ScribbleHiRes (TestMachine & machine, size_t round)
    {
        constexpr uint32_t  kMul = 1664525u;
        constexpr uint32_t  kAdd = 1013904223u;
        constexpr int       kTop = 24;

        uint32_t  seed  = (uint32_t) round + 1;
        Word      start = (Word) ((round * s_kHiResChanged) % s_kHiResBytes);
        Word      i     = 0;



        for (i = 0; i < s_kHiResChanged; i++)
        {
            seed = seed * kMul + kAdd;
            machine.GetMemoryBus().WriteByte ((Word) (s_kHiResPage + (start + i) % s_kHiResBytes), (Byte) (seed >> kTop));
        }
    }


    static uint64_t Fnv (const std::vector<uint32_t> & pixels)
    {
        constexpr uint64_t  kOffsetBasis = 14695981039346656037ULL;
        constexpr uint64_t  kPrime       = 1099511628211ULL;

        uint64_t  hash = kOffsetBasis;



        for (uint32_t pixel : pixels)
        {
            hash = (hash ^ (uint64_t) pixel) * kPrime;
        }

        return hash;
    }


    struct Handoff
    {
        LARGE_INTEGER  submitted = {};
        LARGE_INTEGER  started   = {};
    };


    static void OnHandoff (void * context)
    {
        QueryPerformanceCounter (&static_cast<Handoff *> (context)->started);
    }


    static double MeasureHandoffMs (ThreadPoolWorkQueue & queue)
    {
        Handoff        handoff;
        LARGE_INTEGER  freq  = {};
        double         total = 0.0;
        HRESULT        hr    = S_OK;
        int            i     = 0;



        QueryPerformanceFrequency (&freq);

        for (i = 0; i < s_kCostHandoffs; i++)
        {
            QueryPerformanceCounter (&handoff.submitted);

            hr = queue.Submit (&OnHandoff, &handoff);
            AssertSucceeded (hr, L"Submit");

            queue.WaitAll();

            total += (double) (handoff.started.QuadPart - handoff.submitted.QuadPart) * 1000.0 / (double) freq.QuadPart;
        }

        return total / s_kCostHandoffs;
    }
};
