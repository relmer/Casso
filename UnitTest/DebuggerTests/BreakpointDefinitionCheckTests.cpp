#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/BreakpointDefinitionCheck.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/DebuggerActions.h"

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
        using Type = BreakpointDefinition::Type;

        //  Good definitions of each kind pass, with symbols, flags and the
        //  memory breakpoint's ACCESS and VALUE in their conditions.
        TEST_METHOD (GoodDefinitionsPass)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            for (const BreakpointDefinition & definition : std::vector<BreakpointDefinition>
                 {
                     { Type::Execute,       false, false, "COUT" },
                     { Type::Execute,       false, false, "C283", "", "A == 1" },
                     { Type::ReadWrite,     false, false, "300:3FF", "", "ACCESS == 300" },
                     { Type::Value,         false, false, "300:30F", "41", "VALUE == 41" },
                     { Type::Brk,           false, false, "", "", "P.C" },
                     { Type::BasicLine,     false, false, "100" },
                     { Type::BasicVariable, false, false, "D(3,4)" },
                     { Type::VideoLine,     false, false, "64:6B" },
                     { Type::Beam,          false, false, "64", "1A" },
                     { Type::Beam,          false, false, "VBL" },
                     { Type::Condition,     false, false, "", "", "*FB == 0" },
                 })
            {
                Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError (definition, snapshot, std::nullopt),
                                  TextEncoding::NarrowToWide (DebuggerActions::GetBreakpointEcho (definition)).c_str());
            }
        }



        //  The same execution breakpoint twice is the first one's number,
        //  except for the breakpoint being edited.
        TEST_METHOD (ADuplicateIsAnError)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            Assert::AreEqual (std::string ("Breakpoint #2 is already set at $C283."), BreakpointDefinitionCheck::GetError (BreakpointDefinition::MakeExecute ("C283"), snapshot, std::nullopt));
            Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError (BreakpointDefinition::MakeExecute ("C283"), snapshot, 2));
        }



        //  A bad condition, a line out of range, a scanline past the frame
        //  and a disk breakpoint with no Disk II each have a reason.
        TEST_METHOD (BadDefinitionsHaveAReason)
        {
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            BreakpointDefinition  disk = { Type::Disk, false, false, "", "", "", 0, 0, "11 0" };

            for (const BreakpointDefinition & definition : std::vector<BreakpointDefinition>
                 {
                     { Type::Execute,       false, false, "300", "", "NOSUCH == 1" },
                     { Type::Execute,       false, false, "300", "", "A ==" },
                     { Type::BasicLine,     false, false, "64000" },
                     { Type::BasicLine,     false, false, "ABC" },
                     { Type::BasicVariable, false, false, "2X" },
                     { Type::VideoLine,     false, false, "106" },
                     { Type::Beam,          false, false, "10", "41" },
                     disk,
                     { Type::Value,         false, false, "300", "100" },
                     { Type::Execute },
                 })
            {
                Assert::AreNotEqual (std::string(), BreakpointDefinitionCheck::GetError (definition, snapshot, std::nullopt),
                                     TextEncoding::NarrowToWide (DebuggerActions::GetBreakpointEcho (definition)).c_str());
            }

            snapshot.diskSlot = 6;
            disk.condition = "DISK.QTRACK == 0";
            Assert::AreEqual (std::string(), BreakpointDefinitionCheck::GetError (disk, snapshot, std::nullopt));
        }
    };
}
