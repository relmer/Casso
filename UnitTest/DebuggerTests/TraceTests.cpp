#include "Pch.h"

#include "Debugger/Handlers/TraceHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceTests
//
//  The instruction trace on a real machine: each entry's cycle count and last
//  data access, the ring's capacity, the bus publishing every page while the
//  trace is on and only the watchpoints' pages while it is off, steps adding
//  entries, a machine switch clearing it, and HISTORY showing a window with
//  symbols.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (TraceTests)
    {
    public:

        using Rig = MachineHandlerRig<TraceHandlers>;

        static constexpr Word  kStart     = 0x0300;
        static constexpr Word  kData      = 0x0400;
        static constexpr int   kPageCount = 0x100;

        //  $0300: LDA #$05 / STA $0400 / LDA $0400 / INX / JMP $0300
        static void LoadProgram (Rig & rig)
        {
            rig.Load (kStart, { 0xA9, 0x05, 0x8D, 0x00, 0x04, 0xAD, 0x00, 0x04, 0xE8, 0x4C, 0x00, 0x03 }, kStart);
        }

        static std::vector<TraceRecord> GetAll (Rig & rig)
        {
            std::vector<TraceRecord>  entries;



            rig.target.GetTraceWindow (0, rig.target.GetTraceSize(), entries);
            return entries;
        }



        TEST_METHOD (Entries_CarryCyclesAndTheLastDataAccess)
        {
            Rig                       rig;
            std::vector<TraceRecord>  entries;



            LoadProgram (rig);
            rig.RunOk ("HISTORY ON");

            for (int step = 0; step < 4; step++)
            {
                rig.RunOk ("T");
            }

            entries = GetAll (rig);

            Assert::AreEqual ((size_t) 4, entries.size());
            Assert::AreEqual ((int) kStart, (int) entries[0].pc);
            Assert::IsFalse  (entries[0].hasAccess, L"an immediate load reads only its own bytes");

            Assert::IsTrue   (entries[1].hasAccess);
            Assert::IsTrue   (entries[1].accessIsWrite);
            Assert::AreEqual ((int) kData, (int) entries[1].accessAddress);
            Assert::AreEqual (0x05,        (int) entries[1].accessData);

            Assert::IsTrue   (entries[2].hasAccess);
            Assert::IsFalse  (entries[2].accessIsWrite);
            Assert::AreEqual ((int) kData, (int) entries[2].accessAddress);
            Assert::AreEqual (0x05,        (int) entries[2].accessData);

            Assert::IsFalse  (entries[3].hasAccess, L"INX touches no memory, and the next opcode fetch is not its access");

            Assert::AreEqual ((uint64_t) 2, entries[1].cycles - entries[0].cycles, L"LDA # takes two cycles");
            Assert::AreEqual ((uint64_t) 4, entries[2].cycles - entries[1].cycles, L"STA abs takes four");
            Assert::AreEqual ((uint64_t) 4, entries[3].cycles - entries[2].cycles, L"LDA abs takes four");
        }



        TEST_METHOD (Ring_KeepsTheNewest100000)
        {
            static constexpr uint64_t  kExtra     = 10;
            static constexpr uint64_t  kJmpCycles = 3;
            Rig                        rig;
            std::vector<TraceRecord>   last;
            std::vector<TraceRecord>   first;
            size_t                     size       = 0;



            rig.Load (kStart, { 0x4C, 0x00, 0x03 }, kStart);
            rig.RunOk ("HISTORY ON");
            rig.machine.RunCycles ((TraceController::kCapacity + kExtra) * kJmpCycles);

            size = rig.target.GetTraceSize();
            Assert::AreEqual (TraceController::kCapacity, size);
            Assert::IsTrue   (rig.machine.GetCpu()->GetTraceCount() > (uint64_t) TraceController::kCapacity, L"older entries were dropped, not refused");

            rig.target.GetTraceWindow (0,        1, first);
            rig.target.GetTraceWindow (size - 1, 1, last);

            Assert::AreEqual ((size_t) 1, first.size());
            Assert::AreEqual ((size_t) 1, last.size());
            Assert::AreEqual ((uint64_t) (TraceController::kCapacity - 1) * kJmpCycles, last[0].cycles - first[0].cycles,
                              L"consecutive entries, oldest first");
        }



        TEST_METHOD (On_WatchesEveryPage_OffLeavesTheWatchpointsAlone)
        {
            Rig            rig;
            MemoryBus    & bus   = rig.machine.GetMemoryBus();
            WatchedPages   pages = {};



            pages[kData >> 8] = true;
            rig.target.SetWatchedPages (pages);
            Assert::AreEqual (1, bus.GetWatchedPageCount());

            rig.RunOk ("HISTORY ON");
            Assert::AreEqual (kPageCount, bus.GetWatchedPageCount());
            Assert::IsTrue   (rig.machine.GetCpu()->IsTraceEnabled());
            Assert::IsTrue   (bus.GetReadPageTable()[kStart >> 8] == nullptr, L"a traced page takes the watched path");

            rig.RunOk ("HISTORY OFF");
            Assert::AreEqual (1, bus.GetWatchedPageCount(), L"the watchpoint's page alone");
            Assert::IsFalse  (rig.machine.GetCpu()->IsTraceEnabled(), L"the CPU's trace gate is closed");
            Assert::IsTrue   (bus.GetReadPageTable()[kStart >> 8] != nullptr, L"an untraced page is published again");
            Assert::IsTrue   (bus.GetReadPageTable()[kData >> 8]  == nullptr, L"the watched page stays watched");
        }



        TEST_METHOD (Off_KeepsTheEntries_StepsAppend)
        {
            Rig  rig;



            LoadProgram (rig);
            rig.RunOk ("HISTORY ON");
            rig.RunOk ("T");
            rig.RunOk ("T");
            Assert::AreEqual ((size_t) 2, rig.target.GetTraceSize());

            rig.RunOk ("T");
            Assert::AreEqual ((size_t) 3, rig.target.GetTraceSize(), L"each step adds its instruction");

            rig.RunOk ("HISTORY OFF");
            rig.RunOk ("T");
            Assert::AreEqual ((size_t) 3, rig.target.GetTraceSize(), L"nothing is added while off, and nothing is lost");
        }



        TEST_METHOD (MachineSwitch_ClearsTheTrace)
        {
            Rig  rig;



            LoadProgram (rig);
            rig.RunOk ("HISTORY ON");
            rig.RunOk ("T");

            rig.session.OnMachineChanged ("Apple //e");

            Assert::IsFalse  (rig.target.IsTraceOn());
            Assert::AreEqual ((size_t) 0, rig.target.GetTraceSize());
            Assert::AreEqual (0, rig.machine.GetMemoryBus().GetWatchedPageCount());
        }



        TEST_METHOD (History_ShowsAWindowWithSymbols)
        {
            Rig                       rig;
            std::vector<std::string>  lines;



            LoadProgram (rig);
            rig.session.GetSymbols().Add (SymbolTableId::User, "START",  kStart);
            rig.session.GetSymbols().Add (SymbolTableId::User, "SCREEN", kData);

            rig.RunOk ("HISTORY ON");

            for (int step = 0; step < 4; step++)
            {
                rig.RunOk ("T");
            }

            lines = rig.RunOk ("HISTORY 0 2").text;

            Assert::AreEqual ((size_t) 3, lines.size());
            Assert::AreEqual (std::string ("Trace on, 4 entries retained."), lines[0]);
            Assert::IsTrue   (lines[1].find ("0300 START") != std::string::npos,     Widen (lines[1]).c_str());
            Assert::IsTrue   (lines[1].find ("LDA #$05")   != std::string::npos,     Widen (lines[1]).c_str());
            Assert::IsTrue   (lines[2].find ("STA $0400")  != std::string::npos,     Widen (lines[2]).c_str());
            Assert::IsTrue   (lines[2].find ("W 0400=05 SCREEN") != std::string::npos, Widen (lines[2]).c_str());

            lines = rig.RunOk ("HISTORY").text;
            Assert::AreEqual ((size_t) 5, lines.size(), L"a bare HISTORY shows the newest, all four here");
        }

    private:

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };
}
