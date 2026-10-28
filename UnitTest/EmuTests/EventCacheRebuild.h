#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/ScratchEventReplayer.h"
#include "HResultAssert.h"
#include "TestMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EventCacheRebuild
//
//  Checks a debug event log's cache against a rebuild of the whole of
//  history on a scratch machine: each stretch's entries match the cache's
//  one for one.
//
////////////////////////////////////////////////////////////////////////////////

class EventCacheRebuild
{
public:
    //  Field by field where the event type has an equality operator: an
    //  IoEvent has padding bytes, which no field sets, so two equal events can
    //  differ there. The Disk II and input events contain unions, and are
    //  compared byte for byte.
    template <typename T>
    static bool  IsSameEvent (const T & a, const T & b)
    {
        if constexpr (std::equality_comparable<T>)
        {
            return a == b;
        }
        else
        {
            return memcmp (&a, &b, sizeof (T)) == 0;
        }
    }


    template <typename T>
    static bool  IsSameEntry (const DebugEventEntry<T> & a, const DebugEventEntry<T> & b)
    {
        return a.kind == b.kind && a.position == b.position && a.cycle == b.cycle && IsSameEvent (a.event, b.event);
    }


    //  The count of the cache's entries is added to outEvents.
    template <typename T>
    static void  AssertMatches (TestMachine & machine, ReverseController & controller, const DebugEventCache<T> & cache, std::unique_ptr<IDebugEventProbe<T>> probe, size_t & outEvents)
    {
        using namespace Microsoft::VisualStudio::CppUnitTestFramework;

        DebugEventHistory                        history  (machine);
        std::vector<DebugEventBounds>            stretches;
        DebugEventJob                            job;
        ScratchEventReplayer<T>                  replayer (std::move (probe));
        std::vector<DebugEventStretchResult<T>>  results;
        std::vector<DebugEventEntry<T>>          live;
        HRESULT                                  hr       = S_OK;
        size_t                                   i        = 0;



        history.Attach (&controller.GetKeyframes());
        history.GetStretches (stretches);
        std::ranges::reverse (stretches);

        hr = history.BuildJob (stretches, job);
        AssertSucceeded (hr, L"BuildJob");

        replayer.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

        hr = replayer.Rebuild (job, results);
        AssertSucceeded (hr, L"Rebuild");

        for (const DebugEventStretchResult<T> & result : results)
        {
            live.clear();
            cache.CopySpan (result.startPosition, result.endPosition, live);

            Assert::AreEqual (live.size(), result.entries.size(), std::format (L"the stretch from {}", result.startPosition).c_str());

            for (i = 0; i < live.size(); i++)
            {
                Assert::IsTrue (IsSameEntry (live[i], result.entries[i]), std::format (L"entry {} of the stretch from {}", i, result.startPosition).c_str());
            }

            outEvents += live.size();
        }
    }
};
