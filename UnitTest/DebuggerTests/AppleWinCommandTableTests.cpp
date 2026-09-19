#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommandTableTests
//
//  The AppleWin name table swept against the name list in the spec, in both
//  directions: every listed name resolves, and every engine verb has a name
//  unless it is reached another way.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinCommandTableTests)
    {
    public:

        // The spec's "AppleWin command coverage" list, by availability.
        static constexpr const char * kHeadlessNames[] =
        {
            "A",
            "=", "G", "GG", "IN", "KEY", "JSR", "NOP", "OUT", "LBR", "PROFILE", "R", "POP", "PPOP", "PUSH", "P", "RTS", "T", "TF", "TL", "U",
            "BM", "BMA", "BMC", "BML", "BMG", "BMSAVE",
            "BRK", "BRKOP", "BRKINT", "BP", "BPA", "BPR", "BPX", "BPIO", "BPM", "BPMR", "BPMW", "BPMV", "BPC", "BPD", "BPEDIT", "BPE", "BPL", "BPSAVE", "BPCHANGE",
            "BENCHMARK", "DISASM", "LOAD", "SAVE", "PWD", "CD",
            "CYCLES", "RCC",
            "Z", "X", "B", "DB", "DB2", "DB4", "DB8", "DW", "DW2", "DW4", "ASC", "DF", "DA",
            "DISK",
            "CL", "CLC", "CLZ", "CLI", "CLD", "CLB", "CLR", "CLV", "CLN", "SE", "SEC", "SEZ", "SEI", "SED", "SEB", "SER", "SEV", "SEN",
            "?", "HELP", "VERSION", "MOTD",
            "MC", "ME", "MEB", "MEW", "BLOAD", "M", "BSAVE", "S", "@", "SH", "F", "TSAVE",
            "CALC", "ECHO", "LOG", "PRINT", "PRINTF", "RUN",
            "SYM", "SYMMAIN", "SYMBASIC", "SYMASM", "SYMUSER", "SYMUSER2", "SYMSRC", "SYMSRC2", "SYMDOS33", "SYMPRODOS", "SYMINFO", "SYMLIST",
            "W", "WA", "WC", "WD", "WE", "WL", "WSAVE",
            "ZP", "ZP0", "ZP1", "ZP2", "ZP3", "ZP4", "ZP5", "ZP6", "ZP7", "ZPA", "ZPC", "ZPD", "ZPE", "ZPL", "ZPSAVE",
            "STARTUP",
            "INPUT", "RC", "RZ", "RI", "RD", "RB", "RR", "RV", "RN", "SC", "SZ", "SI", "SD", "SB", "SR", "SV", "SN",
            "D", "ME8", "ME16", "MM", "MS", "P0", "P1", "P2", "P3", "P4", "REGISTER", "TRACE", "SYMDOS", "SYMPRO", "ZAP",
            "BENCH", "EXITBENCH", "MDB",
            "BPV", "VIDEOINFO",
            "MODE", "PAUSE", "BUDGET", "SWITCHES", "STACK", "PATCH", "SRC", "SKIP", "CALLS", "HISTORY", "PANEL",
            "OUTPUT",
        };

        static constexpr const char * kWindowOnlyNames[] =
        {
            ".", "RET", "^", "V", "->", "PAGEUP", "PAGEUP256", "PAGEUP4K", "PAGEDN", "PAGEDOWN256", "PAGEDOWN4K",
            "WIN", "WINDOW", "CODE", "CODE1", "CODE2", "CONSOLE", "DATA", "DATA1", "DATA2", "SOURCE1", "SOURCE2", "\\",
            "MD1", "MD2", "MA1", "MA2", "MT1", "MT2", "M1", "M2",
            "TEXT", "TEXT1", "TEXT2", "TEXT80", "TEXT81", "TEXT82", "TEXT40", "TEXT41", "TEXT42", "GR", "GR1", "GR2",
            "DGR", "DGR1", "DGR2", "HGR", "HGR0", "HGR1", "HGR2", "HGR3", "HGR4", "HGR5", "HGR6", "HGR7", "HGR8", "DHGR", "DHGR1", "DHGR2",
            "BW", "COLOR", "FONT", "HCOLOR", "MONO",
        };

        static constexpr const char * kNotAvailableNames[] = { "SHR", "SOURCE", "SYNC", "NTSC" };

        // Verbs no AppleWin name produces directly: Monitor-mode forms, and
        // operations reached through another command's arguments (R A=41,
        // SYM name = addr, SYM file, MODE MONITOR, PANEL name).
        static constexpr DebugVerb kVerbsWithoutName[] =
        {
            DebugVerb::None,
            DebugVerb::SetRegister,
            DebugVerb::SetConditionalBreakpoint,
            DebugVerb::LoadSymbols,
            DebugVerb::SaveSymbols,
            DebugVerb::ClearSymbols,
            DebugVerb::EnableSymbols,
            DebugVerb::AddSymbol,
            DebugVerb::RemoveSymbol,
            DebugVerb::SetMode,
            DebugVerb::SetOutputFormat,
            DebugVerb::SetSourceStepping,
            DebugVerb::SetSourceBreakpoint,
            DebugVerb::AddStepFilter,
            DebugVerb::RemoveStepFilter,
            DebugVerb::ClearStepFilter,
            DebugVerb::SetCallStackMode,
            DebugVerb::SetHistory,
            DebugVerb::SaveHistory,
            DebugVerb::OpenPanel,
            DebugVerb::ClosePanel,
            DebugVerb::Trace,
            DebugVerb::Examine,
            DebugVerb::Deposit,
            DebugVerb::List,
            DebugVerb::Verify,
            DebugVerb::Arithmetic,
            DebugVerb::SetInverse,
            DebugVerb::SetNormal,
            DebugVerb::SetInputSlot,
            DebugVerb::SetOutputSlot,
            DebugVerb::BasicColdStart,
            DebugVerb::BasicWarmStart,
            DebugVerb::UserVector,
            DebugVerb::ShowRegistersForEdit,
            DebugVerb::EditRegisters,
            DebugVerb::ReadFile,
            DebugVerb::WriteFile,
        };



        static size_t ExpectAvailability (std::span<const char * const> names, CommandAvailability availability)
        {
            size_t checked = 0;



            for (const char * name : names)
            {
                const AppleWinCommand * command = AppleWinCommandTable::Find (name);
                std::string             narrow (name);
                std::wstring            where  (narrow.begin(), narrow.end());



                Assert::IsNotNull (command, where.c_str());
                Assert::AreEqual  ((int) availability, (int) command->availability, where.c_str());
                ++checked;
            }

            return checked;
        }



        TEST_METHOD (ListedNames_ResolveWithTheirAvailability)
        {
            size_t headless     = ExpectAvailability (kHeadlessNames,     CommandAvailability::Headless);
            size_t windowOnly   = ExpectAvailability (kWindowOnlyNames,   CommandAvailability::WindowOnly);
            size_t notAvailable = ExpectAvailability (kNotAvailableNames, CommandAvailability::NotAvailable);



            Logger::WriteMessage (std::format ("AppleWin names: {} headless, {} window-only, {} not available\n",
                                               headless, windowOnly, notAvailable).c_str());

            Assert::IsTrue   (headless > 0);
            Assert::AreEqual (headless + windowOnly + notAvailable, AppleWinCommandTable::GetAll().size());
        }



        TEST_METHOD (NotAvailable_HasReason)
        {
            for (const char * name : kNotAvailableNames)
            {
                const AppleWinCommand * command = AppleWinCommandTable::Find (name);



                Assert::IsNotNull (command);
                Assert::IsNotNull (command->reason);
                Assert::IsTrue    (std::string (command->reason).starts_with (name));
            }
        }



        TEST_METHOD (Aliases_ResolveToTargetVerb)
        {
            size_t aliases = 0;



            for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
            {
                const AppleWinCommand * target = nullptr;



                if (command.aliasOf == nullptr)
                {
                    continue;
                }

                target = AppleWinCommandTable::Find (command.aliasOf);

                Assert::IsNotNull (target);
                Assert::IsNull    (target->aliasOf);
                Assert::AreEqual  ((int) target->verb, (int) command.verb);
                ++aliases;
            }

            Assert::IsTrue (aliases > 0);
        }



        TEST_METHOD (EveryVerb_HasAName)
        {
            std::set<DebugVerb>  named;
            std::wstring         mismatches;



            for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
            {
                if (command.availability != CommandAvailability::NotAvailable)
                {
                    named.insert (command.verb);
                }
            }

            for (int i = 0; i < (int) DebugVerb::Count; ++i)
            {
                DebugVerb verb     = (DebugVerb) i;
                bool      isExempt = std::find (std::begin (kVerbsWithoutName), std::end (kVerbsWithoutName), verb) != std::end (kVerbsWithoutName);



                if (isExempt == named.contains (verb))
                {
                    mismatches += std::format (L"{}{} ({})", mismatches.empty() ? L"" : L", ", i, isExempt ? L"exempt but named" : L"no name");
                }
            }

            Assert::IsTrue (mismatches.empty(), (L"verbs: " + mismatches).c_str());
        }



        TEST_METHOD (Lookup_IgnoresCase_UnknownIsNull)
        {
            Assert::IsNotNull (AppleWinCommandTable::Find ("bpmr"));
            Assert::IsNotNull (AppleWinCommandTable::Find ("BpMr"));
            Assert::IsNotNull (AppleWinCommandTable::Find ("v"));
            Assert::IsNull    (AppleWinCommandTable::Find ("FROB"));
            Assert::IsNull    (AppleWinCommandTable::Find (""));
        }
    };
}
