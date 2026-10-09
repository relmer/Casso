#include "Pch.h"

#include "Ui/Debugger/BreakpointColumns.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumnsTests
//
//  The breakpoints pane's columns: Name, Condition, Hit count, Kind, Symbol,
//  When hit, Function, File, Address and Data, built from the snapshot; the
//  order a click on a heading sorts the rows into; and the choice of columns
//  kept with the open views.
//
////////////////////////////////////////////////////////////////////////////////

namespace BreakpointColumnsTests
{
    using Column = BreakpointColumns::Column;



    static DebuggerViewSnapshot::BreakpointLine MakeLine (int id, BreakpointKind kind, Word address, Word last = 0)
    {
        DebuggerViewSnapshot::BreakpointLine  bp;



        bp.id           = id;
        bp.address      = address;
        bp.info.id      = id;
        bp.info.kind    = kind;
        bp.info.address = address;
        bp.info.last    = last;
        return bp;
    }



    static std::string Cell (const DebuggerViewSnapshot & snapshot, size_t index, Column column)
    {
        return BreakpointColumns::GetCells (snapshot, snapshot.breakpoints[index])[(size_t) column];
    }



    TEST_CLASS (BreakpointColumnsTests)
    {
    public:

        TEST_METHOD (TheHeadingsAreInSentenceCase)
        {
            Assert::AreEqual (std::wstring (L"Hit count"), BreakpointColumns::GetHeading (Column::HitCount));
            Assert::AreEqual (std::wstring (L"Kind"),      BreakpointColumns::GetHeading (Column::Kind));
            Assert::AreEqual (std::wstring (L"Symbol"),    BreakpointColumns::GetHeading (Column::Symbol));
            Assert::AreEqual (std::wstring (L"When hit"),  BreakpointColumns::GetHeading (Column::WhenHit));
        }


        TEST_METHOD (AnAddressBreakpointShowsItsSymbolAndAddress)
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Address, 0xFA62));
            snapshot.breakpoints[0].label          = "RESET";
            snapshot.breakpoints[0].info.condition = "A=41";
            snapshot.breakpoints[0].info.hits      = 3;

            Assert::AreEqual (std::string ("RESET $FA62"), Cell (snapshot, 0, Column::Name));
            Assert::AreEqual (std::string ("A=41"),        Cell (snapshot, 0, Column::Condition));
            Assert::AreEqual (std::string ("3"),           Cell (snapshot, 0, Column::HitCount));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 0, Column::Data));
            Assert::AreEqual (std::string ("$FA62"),       Cell (snapshot, 0, Column::Address));
            Assert::AreEqual (std::string ("RESET"),       Cell (snapshot, 0, Column::Symbol));
            Assert::AreEqual (std::string ("Address"),     Cell (snapshot, 0, Column::Kind), L"a symbol at the address makes no function breakpoint");
            Assert::AreEqual (std::string (""),            Cell (snapshot, 0, Column::File));
            Assert::AreEqual (std::string ("Break"),       Cell (snapshot, 0, Column::WhenHit));
        }


        TEST_METHOD (ASourceBreakpointShowsItsFileAndLine)
        {
            DebuggerViewSnapshot               snapshot;
            DebuggerViewSnapshot::SourceState  source;
            DebugSourceFile                    file;



            file.id   = 4;
            file.name = "src/main.s";
            source.files.push_back (file);
            source.breakpointLines.emplace_back (4, 12, 7);
            snapshot.source = source;
            snapshot.breakpoints.push_back (MakeLine (7, BreakpointKind::Address, 0x0803));

            Assert::AreEqual (std::string ("src/main.s, line 12"), Cell (snapshot, 0, Column::Name));
            Assert::AreEqual (std::string ("src/main.s:12"),       Cell (snapshot, 0, Column::File));
            Assert::AreEqual (std::string ("$0803"),               Cell (snapshot, 0, Column::Address));
            Assert::AreEqual (std::string ("Source line"),         Cell (snapshot, 0, Column::Kind));
        }


        TEST_METHOD (EachKindSaysWhatItIs)
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Memory, 0x0300, 0x03FF));
            snapshot.breakpoints.push_back (MakeLine (2, BreakpointKind::Opcode, 0));
            snapshot.breakpoints.push_back (MakeLine (3, BreakpointKind::Register, 0));
            snapshot.breakpoints.push_back (MakeLine (4, BreakpointKind::Io, 0xC030));
            snapshot.breakpoints.push_back (MakeLine (5, BreakpointKind::Brk, 0));
            snapshot.breakpoints.push_back (MakeLine (6, BreakpointKind::Interrupt, 0));
            snapshot.breakpoints[0].info.access    = WatchAccess::Write;
            snapshot.breakpoints[1].info.opcode    = 0xEA;
            snapshot.breakpoints[2].info.condition = "X=00";

            Assert::AreEqual (std::string ("$0300-$03FF"), Cell (snapshot, 0, Column::Name));
            Assert::AreEqual (std::string ("Write $0300-$03FF"), Cell (snapshot, 0, Column::Data));
            Assert::AreEqual (std::string ("Opcode $EA"),  Cell (snapshot, 1, Column::Name));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 1, Column::Address), L"an opcode stops anywhere");
            Assert::AreEqual (std::string ("X=00"),        Cell (snapshot, 2, Column::Name));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 2, Column::Condition), L"the condition is the name");
            Assert::AreEqual (std::string ("I/O $C030"),   Cell (snapshot, 3, Column::Name));
            Assert::AreEqual (std::string ("BRK"),         Cell (snapshot, 4, Column::Name));
            Assert::AreEqual (std::string ("Interrupt"),   Cell (snapshot, 5, Column::Name));
        }


        TEST_METHOD (KindGivesEachSortOfBreakpointTheEngineHolds)
        {
            DebuggerViewSnapshot               snapshot;
            DebuggerViewSnapshot::SourceState  source;
            DebugSourceFile                    file;
            std::vector<std::string>           expected =
            {
                "Address", "Address", "Source line", "Address", "Data read", "Data write", "Data read or write",
                "Data value", "Register condition", "Opcode", "I/O", "BRK", "Interrupt",
            };



            file.id   = 1;
            file.name = "main.s";
            source.files.push_back (file);
            source.breakpointLines.emplace_back (1, 20, 3);
            snapshot.source = source;

            snapshot.breakpoints.push_back (MakeLine (1,  BreakpointKind::Address,     0x0300));
            snapshot.breakpoints.push_back (MakeLine (2,  BreakpointKind::Address,     0x0300, 0x03FF));
            snapshot.breakpoints.push_back (MakeLine (3,  BreakpointKind::Address,     0x0803));
            snapshot.breakpoints.push_back (MakeLine (4,  BreakpointKind::Address,     0xFDED));
            snapshot.breakpoints.push_back (MakeLine (5,  BreakpointKind::Memory,      0x0400));
            snapshot.breakpoints.push_back (MakeLine (6,  BreakpointKind::Memory,      0x0400));
            snapshot.breakpoints.push_back (MakeLine (7,  BreakpointKind::Memory,      0x0400));
            snapshot.breakpoints.push_back (MakeLine (8,  BreakpointKind::MemoryValue, 0x0400));
            snapshot.breakpoints.push_back (MakeLine (9,  BreakpointKind::Register,    0));
            snapshot.breakpoints.push_back (MakeLine (10, BreakpointKind::Opcode,      0));
            snapshot.breakpoints.push_back (MakeLine (11, BreakpointKind::Io,          0xC030));
            snapshot.breakpoints.push_back (MakeLine (12, BreakpointKind::Brk,         0));
            snapshot.breakpoints.push_back (MakeLine (13, BreakpointKind::Interrupt,   0));

            //  A symbol at the address makes no execution breakpoint a
            //  function, whether at one address or at a range's start, and a
            //  breakpoint on a source line stays one when its address has a
            //  symbol.
            snapshot.breakpoints[1].label       = "BUFFER";
            snapshot.breakpoints[2].label       = "START";
            snapshot.breakpoints[3].label       = "COUT";
            snapshot.breakpoints[4].info.access = WatchAccess::Read;
            snapshot.breakpoints[5].info.access = WatchAccess::Write;
            snapshot.breakpoints[6].info.access = WatchAccess::ReadWrite;

            for (size_t i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual (expected[i], Cell (snapshot, i, Column::Kind), std::format (L"breakpoint {}", i + 1).c_str());
            }
        }


        TEST_METHOD (EveryKindHasItsOwnText)
        {
            std::vector<std::string>  expected =
            {
                "Address", "Source line", "Data read", "Data write", "Data read or write",
                "Data value", "Register condition", "Opcode", "I/O", "BRK", "Interrupt",
            };
            std::set<std::string>     texts;



            Assert::AreEqual (expected.size(), (size_t) BreakpointColumns::RowKind::Count);

            for (size_t i = 0; i < (size_t) BreakpointColumns::RowKind::Count; i++)
            {
                std::string  text = BreakpointColumns::GetKindText ((BreakpointColumns::RowKind) i);

                Assert::AreEqual (expected[i], text);
                texts.insert (text);
            }

            Assert::AreEqual (expected.size(), texts.size(), L"no two kinds share a text");
        }


        TEST_METHOD (WhenHitAndHitCountFollowWhatItDoes)
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Address, 0x0300));
            snapshot.breakpoints.push_back (MakeLine (2, BreakpointKind::Address, 0x0310));
            snapshot.breakpoints[0].info.temporary = true;
            snapshot.breakpoints[1].info.stops     = false;
            snapshot.breakpoints[1].info.hits      = 5;

            Assert::AreEqual (std::string ("Break once"),       Cell (snapshot, 0, Column::WhenHit));
            Assert::AreEqual (std::string ("Count"),            Cell (snapshot, 1, Column::WhenHit));
            Assert::AreEqual (std::string ("5 (count only)"),   Cell (snapshot, 1, Column::HitCount));
        }


        TEST_METHOD (AHeadingSortsByItsColumn)
        {
            DebuggerViewSnapshot  snapshot;
            std::vector<size_t>   order;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Address, 0x0900));
            snapshot.breakpoints.push_back (MakeLine (2, BreakpointKind::Address, 0x0300));
            snapshot.breakpoints.push_back (MakeLine (3, BreakpointKind::Address, 0x0600));
            snapshot.breakpoints[0].info.hits = 10;
            snapshot.breakpoints[1].info.hits = 2;
            snapshot.breakpoints[2].info.hits = 7;
            snapshot.breakpoints[0].label     = "beta";
            snapshot.breakpoints[1].label     = "Gamma";
            snapshot.breakpoints[2].label     = "alpha";

            order = BreakpointColumns::GetOrder (snapshot, Column::Address, false);
            Assert::IsTrue (order == std::vector<size_t> { 1, 2, 0 }, L"by address");

            order = BreakpointColumns::GetOrder (snapshot, Column::HitCount, true);
            Assert::IsTrue (order == std::vector<size_t> { 0, 2, 1 }, L"by hits, most first, as numbers");

            order = BreakpointColumns::GetOrder (snapshot, Column::Symbol, false);
            Assert::IsTrue (order == std::vector<size_t> { 2, 0, 1 }, L"by symbol, ignoring case");
        }


        TEST_METHOD (KindSortsByKindThenTheEnginesOrder)
        {
            DebuggerViewSnapshot  snapshot;
            std::vector<size_t>   order;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Memory,  0x0400));
            snapshot.breakpoints.push_back (MakeLine (2, BreakpointKind::Address, 0x0300));
            snapshot.breakpoints.push_back (MakeLine (3, BreakpointKind::Brk,     0));
            snapshot.breakpoints.push_back (MakeLine (4, BreakpointKind::Address, 0x0200));
            snapshot.breakpoints.push_back (MakeLine (5, BreakpointKind::Memory,  0x0100));
            snapshot.breakpoints.push_back (MakeLine (6, BreakpointKind::Opcode,  0));
            snapshot.breakpoints[0].info.access = WatchAccess::Write;
            snapshot.breakpoints[4].info.access = WatchAccess::Write;

            //  By the kinds' order, not their text, which would put BRK second.
            order = BreakpointColumns::GetOrder (snapshot, Column::Kind, false);
            Assert::IsTrue (order == std::vector<size_t> { 1, 3, 0, 4, 5, 2 }, L"Address, Data write, Opcode, BRK; each kind in the engine's order");

            order = BreakpointColumns::GetOrder (snapshot, Column::Kind, true);
            Assert::IsTrue (order == std::vector<size_t> { 2, 5, 0, 4, 1, 3 }, L"the kinds turned around, each still in the engine's order");
        }


        TEST_METHOD (GoToSourceCodeFindsTheLineSetOrTheLineAtTheAddress)
        {
            DebuggerViewSnapshot               snapshot;
            DebuggerViewSnapshot::SourceState  source;
            int                                fileId = -1;
            int                                line   = 0;



            source.breakpointLines.emplace_back (2, 30, 1);
            source.lineAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                std::map<std::pair<int, int>, Word> { { { 3, 41 }, (Word) 0x0900 } });
            snapshot.source = source;
            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Address, 0x0800));
            snapshot.breakpoints.push_back (MakeLine (2, BreakpointKind::Address, 0x0900));
            snapshot.breakpoints.push_back (MakeLine (3, BreakpointKind::Address, 0x0A00));
            snapshot.breakpoints.push_back (MakeLine (4, BreakpointKind::Brk, 0x0900));

            Assert::IsTrue   (BreakpointColumns::TryGetSourcePlace (snapshot, snapshot.breakpoints[0], fileId, line), L"set from source");
            Assert::AreEqual (2,  fileId);
            Assert::AreEqual (30, line);

            Assert::IsTrue   (BreakpointColumns::TryGetSourcePlace (snapshot, snapshot.breakpoints[1], fileId, line), L"at an address with a line");
            Assert::AreEqual (3,  fileId);
            Assert::AreEqual (41, line);

            Assert::IsFalse  (BreakpointColumns::TryGetSourcePlace (snapshot, snapshot.breakpoints[2], fileId, line), L"no line produced that code");
            Assert::IsFalse  (BreakpointColumns::TryGetSourcePlace (snapshot, snapshot.breakpoints[3], fileId, line), L"BRK stops anywhere");
            Assert::IsFalse  (BreakpointColumns::HasAddress (snapshot.breakpoints[3].info), L"so it has no disassembly to go to");
        }


        TEST_METHOD (TheDefaultColumnsAreNameConditionHitCountAndKind)
        {
            BreakpointColumns::Shown  shown = BreakpointColumns::GetDefaultShown();



            for (size_t i = 0; i < BreakpointColumns::kCount; i++)
            {
                Column  column   = (Column) i;
                bool    expected = column == Column::Name || column == Column::Condition || column == Column::HitCount || column == Column::Kind;

                Assert::AreEqual (expected, shown[i], BreakpointColumns::GetHeading (column).c_str());
            }

            Assert::AreEqual (std::string (""), BreakpointColumns::FormatShown (shown), L"the default is not written");
            Assert::IsTrue  (BreakpointColumns::ParseShown ("panel=disk2 code2=0300") == shown, L"nothing saved is the default");
        }


        TEST_METHOD (TheChosenColumnsSurviveTheSavedText)
        {
            BreakpointColumns::Shown  shown = BreakpointColumns::GetDefaultShown();
            std::string               text;



            shown[(size_t) Column::HitCount] = false;
            shown[(size_t) Column::Kind]     = false;
            shown[(size_t) Column::Symbol]   = true;
            shown[(size_t) Column::File]     = true;
            text                             = "follow=1" + BreakpointColumns::FormatShown (shown) + " memory2=0400";

            Assert::IsTrue (BreakpointColumns::ParseShown (text) == shown);
            Assert::IsTrue (text.find (" bpcols2=") != std::string::npos, L"written under the token whose bits follow the columns' order now");
        }


        TEST_METHOD (AChoiceSavedWithLabelsShowsSymbolAndOneWithFilterDropsIt)
        {
            //  The old token's bits: Name 0, Condition 1, Labels 2, Hit count 3,
            //  Filter 4, When hit 5, Function 6, File 7, Address 8, Data 9.
            BreakpointColumns::Shown  shown = BreakpointColumns::ParseShown ("follow=1 bpcols=215 memory2=0400");



            Assert::IsTrue  (shown[(size_t) Column::Name]);
            Assert::IsTrue  (shown[(size_t) Column::Symbol],    L"Labels was chosen");
            Assert::IsTrue  (shown[(size_t) Column::Data],      L"Data was chosen");
            Assert::IsTrue  (shown[(size_t) Column::Kind],      L"Kind is new, so it shows as it does by default");
            Assert::IsFalse (shown[(size_t) Column::Condition], L"Condition was hidden");
            Assert::IsFalse (shown[(size_t) Column::HitCount],  L"Hit count was hidden");
            Assert::IsFalse (shown[(size_t) Column::WhenHit],   L"Filter's bit does not move to the column after it");
            Assert::IsFalse (shown[(size_t) Column::Function]);
            Assert::IsFalse (shown[(size_t) Column::File]);
            Assert::IsFalse (shown[(size_t) Column::Address]);
        }


        TEST_METHOD (AnOldChoiceOfTheDefaultsAndFilterComesBackAsTheDefault)
        {
            //  Name, Condition, Hit count and Filter, under the old token.
            BreakpointColumns::Shown  shown = BreakpointColumns::ParseShown ("bpcols=1B");



            Assert::IsTrue   (shown == BreakpointColumns::GetDefaultShown());
            Assert::AreEqual (std::string (""), BreakpointColumns::FormatShown (shown));
        }


        TEST_METHOD (AChoiceSavedSinceWinsOverAnOldOne)
        {
            BreakpointColumns::Shown  shown = BreakpointColumns::GetDefaultShown();
            std::string               saved;



            shown[(size_t) Column::Address] = true;
            saved                           = BreakpointColumns::FormatShown (shown);

            Assert::IsTrue (BreakpointColumns::ParseShown ("bpcols=215" + saved) == shown,             L"the old token first");
            Assert::IsTrue (BreakpointColumns::ParseShown (saved.substr (1) + " bpcols=215") == shown, L"the old token last");
        }
    };
}
