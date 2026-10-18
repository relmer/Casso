#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/DebugEventLogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventLogScrollTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventLogScrollTests
    //
    //  The Disk II log scrolled back a screen at a time through a DOS 3.3
    //  boot it was not open for, on the pool thread the debugger rebuilds on:
    //  each screen the cache and the rows already rebuilt cannot fill is
    //  filled within a quarter second of the request.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventLogScrollTests)
    {
    public:

        //  The rows of a log pane of ordinary height.
        static constexpr size_t  kScreenRows = 40;

        //  The longest wait allowed for a screen the pane has no rows for.
        static constexpr double  kScreenMs = 250.0;

        //  Long enough for any rebuild that is not broken.
        static constexpr double  kGiveUpMs = 10000.0;


        //  Requests the rows before the pane's oldest until the screen above
        //  is full or history begins; the milliseconds that took.
        static double  WaitForScreen (DebugEventLogs & logs, size_t needed, size_t & ioRows, bool & ioIsAtStart)
        {
            DiskLogDelta                           delta;
            std::chrono::steady_clock::time_point  began   = std::chrono::steady_clock::now();
            double                                 elapsed = 0.0;



            logs.RequestEarlier (DebugLogKind::Disk);

            while (ioRows < needed && !ioIsAtStart && elapsed < kGiveUpMs)
            {
                logs.Pump();

                if (logs.TakeDelta (delta))
                {
                    ioRows      += delta.older.size();
                    ioIsAtStart  = delta.isAtStart;

                    //  The pane is at the top of its rows: request the rows before them.
                    if (ioRows < needed && !delta.isRebuilding)
                    {
                        logs.RequestEarlier (DebugLogKind::Disk);
                    }
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (1));

                elapsed = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - began).count();
            }

            return elapsed;
        }


        TEST_METHOD (EachScreenBackThroughABootIsShownWithinAQuarterSecond)
        {
            TestMachine               machine    ("Apple2eEnhanced");
            ReverseController         controller (machine);
            DebugEventLogs            logs       (machine);
            std::vector<std::string>  rows;
            std::vector<double>       waits;
            DiskLogDelta              delta;
            HRESULT                   hr         = S_OK;
            size_t                    rowCount   = 0;
            size_t                    shown      = 0;
            size_t                    screens    = 0;
            bool                      isAtStart  = false;
            bool                      isAtPrompt = false;



            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            GuestSession::Mount (machine, GuestSession::RequireDos33Master());

            hr = controller.Start (ReverseSettings());
            AssertSucceeded (hr, L"Start");

            MachineIdle::RunUntilIdle (machine, GuestSession::kBootCycles);

            isAtPrompt = GuestSession::TryPageToPrompt (machine, rows);
            Assert::IsTrue (isAtPrompt, L"DOS booted to its prompt");

            //  The log starts after the boot, so the boot's events are in
            //  history alone and every screen of them is rebuilt.
            logs.Start (&controller);
            rows = GuestSession::TypeAndCollect (machine, "CATALOG");

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            logs.SetOpen (DebugLogKind::Disk, true);
            logs.TakeDelta (delta);

            rowCount  = delta.newer.size();
            isAtStart = delta.isAtStart;
            shown     = std::min (rowCount, kScreenRows);

            Assert::IsTrue  (rowCount > 0, L"CATALOG ran the disk while the log was open");
            Assert::IsFalse (isAtStart, L"the boot is before the log's first row");

            while (!isAtStart || shown < rowCount)
            {
                if (rowCount < shown + kScreenRows && !isAtStart)
                {
                    waits.push_back (WaitForScreen (logs, shown + kScreenRows, rowCount, isAtStart));
                    Assert::IsTrue (waits.back() < kGiveUpMs, std::format (L"screen {} came in", screens + 1).c_str());
                }

                shown = std::min (rowCount, shown + kScreenRows);
                screens++;
            }

            logs.Stop();

            Assert::IsTrue (screens > 10,   L"the boot is many screens back");
            Assert::IsTrue (!waits.empty(), L"rows were rebuilt");

            std::ranges::sort (waits);

            Logger::WriteMessage (std::format ("{} screens, {} rows; {} waited on a rebuild: median {:.1f} ms, slowest {:.1f} ms\n",
                                               screens, rowCount, waits.size(), waits[waits.size() / 2], waits.back()).c_str());

#ifdef NDEBUG
            Assert::IsTrue (waits.back() <= kScreenMs, std::format (L"slowest screen {:.1f} ms", waits.back()).c_str());
#else
            Logger::WriteMessage ("Debug build: the quarter second is checked in Release only\n");
#endif
        }
    };
}
