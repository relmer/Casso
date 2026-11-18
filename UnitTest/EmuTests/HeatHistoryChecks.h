#pragma once

#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/MachineDebugTarget.h"
#include "Debugger/Reverse/ReverseController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistoryChecks
//
//  The helpers of the heat history tests: the session's size, the totals on
//  the map, and the comparisons of totals and heat with a straight run's.
//
////////////////////////////////////////////////////////////////////////////////

class HeatHistoryChecks
{
public:
    using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

    static constexpr uint64_t  kSliceCycles = 3000;
    static constexpr size_t    kSlices      = 600;
    static constexpr size_t    kFirstPart   = 360;
    static constexpr size_t    kSamples     = 9;
    static constexpr size_t    kShuffle     = 5;
    static constexpr uint64_t  kRunOn       = 2500;

    //  The totals the map shows: none below zero.
    static std::vector<int64_t> GetShown (const AccessHeatMap & map)
    {
        std::vector<int64_t>  totals;



        map.GetTotalsNow (totals);

        for (int64_t & total : totals)
        {
            total = std::max<int64_t> (total, 0);
        }

        return totals;
    }


    static void AssertSameTotals (const std::vector<int64_t> & expected, const std::vector<int64_t> & actual, const std::wstring & where)
    {
        size_t  first   = 0;
        size_t  touched = 0;



        Assert::AreEqual (expected.size(), actual.size(), where.c_str());

        while (first < expected.size() && expected[first] == actual[first])
        {
            first++;
        }

        for (int64_t total : expected)
        {
            touched += (total > 0) ? 1 : 0;
        }

        Assert::IsTrue (first == expected.size(),
                        std::format (L"{}: kind {} address ${:04X} counted {}, a straight count gives {}",
                                     where,
                                     first / AccessHeatMap::kAddressCount,
                                     first % AccessHeatMap::kAddressCount,
                                     (first < actual.size())   ? actual[first]   : -1,
                                     (first < expected.size()) ? expected[first] : -1).c_str());

        Assert::IsTrue (touched > 0, (where + L": the straight count touched nothing, so the comparison proves nothing").c_str());
    }


    //  Heat merged from parts fades in steps of a part rather than of a
    //  frame, which rounds differently; and a heat that went cold at a fold
    //  in a straight run can linger below the cold line in a merge. Neither
    //  shows on the map.
    static void AssertNearHeat (const std::vector<float> & expected, const std::vector<float> & actual, uint64_t at)
    {
        constexpr float  kRelative = 1e-4f;
        size_t           first     = 0;
        size_t           warm      = 0;
        float            allowed   = 0.0f;



        Assert::AreEqual (expected.size(), actual.size(), std::format (L"heat at {}: entries", at).c_str());

        for (first = 0; first < expected.size(); first++)
        {
            allowed = std::max (expected[first] * kRelative, AccessHeatMap::kColdHeat);

            if (std::fabs (expected[first] - actual[first]) > allowed)
            {
                break;
            }
        }

        for (float heat : expected)
        {
            warm += (heat > 0.0f) ? 1 : 0;
        }

        Assert::IsTrue (warm > 0, L"the straight run left no heat, so the comparison proves nothing");
        Assert::IsTrue (first == expected.size(),
                        std::format (L"heat at {}: kind {} address ${:04X} is {}, a straight run gives {}",
                                     at,
                                     first / AccessHeatMap::kAddressCount,
                                     first % AccessHeatMap::kAddressCount,
                                     (first < actual.size())   ? actual[first]   : -1.0f,
                                     (first < expected.size()) ? expected[first] : -1.0f).c_str());
    }


    static bool IsDebugBuild()
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
    }


    static bool IsSanitizedBuild()
    {
#ifdef __SANITIZE_ADDRESS__
        return true;
#else
        return false;
#endif
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatScript
//
//  The reverse rig's scripted session, run so it can stop at any position
//  and carry on from there: each slice feeds the slice's inputs and runs its
//  cycles, as ReverseReplayTests' session does. With a map given, the map is
//  folded at the first instruction at or after every frame boundary, as a
//  heat rebuild folds it.
//
////////////////////////////////////////////////////////////////////////////////

class HeatScript
{
public:
    explicit HeatScript (size_t slices = HeatHistoryChecks::kSlices) : m_slices (slices) {}

    void RunTo (MachineHost & machine, uint64_t endPosition, AccessHeatMap * map = nullptr)
    {
        uint64_t  cycle    = 0;
        uint64_t  frameEnd = 0;



        while (m_slice < m_slices && machine.GetRunner().GetPosition() < endPosition)
        {
            if (!m_isFed)
            {
                ReverseSessionRig::Feed (machine, m_slice);

                m_isFed = true;
                m_spent = 0;
            }

            while (m_spent < HeatHistoryChecks::kSliceCycles && machine.GetRunner().GetPosition() < endPosition)
            {
                cycle    = machine.GetCpu()->GetTotalCycles();
                frameEnd = (cycle / AccessHeatMap::kCyclesPerFrame + 1) * AccessHeatMap::kCyclesPerFrame;

                m_spent += machine.GetRunner().StepOne();

                if (map != nullptr && machine.GetCpu()->GetTotalCycles() >= frameEnd)
                {
                    map->Fold (machine.GetCpu()->GetTotalCycles());
                }
            }

            if (m_spent >= HeatHistoryChecks::kSliceCycles)
            {
                m_slice++;
                m_isFed = false;
            }
        }
    }

    size_t GetSlice() const { return m_slice; }

private:
    size_t    m_slices = 0;
    size_t    m_slice  = 0;
    uint64_t  m_spent  = 0;
    bool      m_isFed  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistoryRig
//
//  A //e running the reverse rig's session with history on, its heat map on
//  from where history begins and stored in that history.
//
////////////////////////////////////////////////////////////////////////////////

class HeatHistoryRig
{
public:
    using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

    TestMachine          machine    { "Apple2e" };
    MachineDebugTarget   target     { machine };
    ReverseController    controller { machine };
    HeatScript           script;

    explicit HeatHistoryRig (const ReverseSettings & settings, IHeatRebuilder * rebuilder = nullptr)
    {
        HRESULT  hr = S_OK;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (settings);
        AssertSucceeded (hr, L"Start");

        target.AttachHistory (&controller.GetKeyframes(), rebuilder);
        controller.GetCapturer().SetObserver (target.GetHistoryObserver());
        target.SetHeatMapOn (true);
    }

    ~HeatHistoryRig()
    {
        controller.GetCapturer().SetObserver (nullptr);
        target.AttachHistory (nullptr, nullptr);
    }

    const AccessHeatMap & GetMap()
    {
        const AccessHeatMap  * map = target.FoldHeatMap();



        Assert::IsNotNull (map, L"the map is on");

        return *map;
    }

    //  A move as the shell makes one: the command, then the map told.
    void Seek (uint64_t position)
    {
        ReverseResult  result;
        HRESULT        hr     = S_OK;



        hr = controller.GetSeeker().SeekToPosition (position, result);
        AssertSucceeded (hr, L"SeekToPosition");
        Assert::AreEqual<uint64_t> (position, machine.GetRunner().GetPosition(), L"the seek lands where asked");

        target.NoteHistoryMoved (false);
    }
};
