#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/BreakpointDefinitionCheck.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheckTests
//
//  The breakpoint dialog's check before it closes: a definition that parses
//  and is in range passes, and a bad one returns a reason.
//
////////////////////////////////////////////////////////////////////////////////

namespace BreakpointDefinitionCheckTests
{
    static DebuggerViewSnapshot MakeSnapshot()
    {
        DebuggerViewSnapshot                                   snapshot;
        DebuggerViewSnapshot::BreakpointLine                   bp;
        std::shared_ptr<DebuggerViewSnapshot::SymbolsState>    symbols = std::make_shared<DebuggerViewSnapshot::SymbolsState>();



        bp.id                = 2;
        bp.address           = 0xC283;
        bp.info.id           = 2;
        bp.info.kind         = BreakpointKind::Address;
        bp.info.address      = 0xC283;
        bp.info.last         = 0xC283;
        snapshot.breakpoints = { bp };

        symbols->symbols.push_back ({ "COUT", 0xFDED });
        snapshot.symbols = symbols;
        return snapshot;
    }



    TEST_CLASS (BreakpointDefinitionCheckTests)
    {
    public:
        //  Good definitions of each kind pass, with symbols, flags and the
        //  memory breakpoint's ACCESS and VALUE in their conditions.
        TEST_METHOD (GoodDefinitionsPass)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            for (const char * definition : { "BP COUT", "BP C283 IF A == 1", "BPM 300:3FF IF ACCESS == 300", "BPMV 300:30F 41 IF VALUE == 41",
                                             "BRK ON IF P.C", "BPBASIC 100", "BPBASICVAR D(3,4)", "BPV 64:6B", "BPBEAM 64 1A", "BPBEAM VBL",
                                             "BPR IF *FB == 0" })
            {
                Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError (definition, snapshot, std::nullopt), TextEncoding::NarrowToWide (definition).c_str());
            }
        }



        //  The same execution breakpoint twice is the first one's number,
        //  except for the breakpoint being edited.
        TEST_METHOD (ADuplicateIsAnError)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            Assert::AreEqual (std::string ("Breakpoint #2 is already set at $C283."), BreakpointDefinitionCheck::GetError ("BP C283", snapshot, std::nullopt));
            Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError ("BP C283", snapshot, 2));
        }



        //  A bad condition, a line out of range, a scanline past the frame
        //  and a disk breakpoint with no Disk II each have a reason.
        TEST_METHOD (BadDefinitionsHaveAReason)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            for (const char * definition : { "BP 300 IF NOSUCH == 1", "BP 300 IF A ==", "BPBASIC 64000", "BPBASIC ABC", "BPBASICVAR 2X",
                                             "BPV 106", "BPBEAM 10 41", "BPDISK READ 11 0", "BPMV 300 100", "BP" })
            {
                Assert::AreNotEqual (std::string(), BreakpointDefinitionCheck::GetError (definition, snapshot, std::nullopt), TextEncoding::NarrowToWide (definition).c_str());
            }

            snapshot.diskSlot = 6;
            Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError ("BPDISK READ 11 0 IF DISK.QTRACK == 0", snapshot, std::nullopt));
        }
    };
}
