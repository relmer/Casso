#include "Pch.h"

#include "Ui/Debugger/BreakpointColumns.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumnsTests
//
//  The breakpoints pane's columns (FR-117): Name, Condition, Hit count,
//  Kind, Address, Label, File and When hit, built from the snapshot, and the
//  order a click on a heading sorts the rows into.
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
            Assert::AreEqual (std::wstring (L"When hit"),  BreakpointColumns::GetHeading (Column::WhenHit));
        }


        TEST_METHOD (AnAddressBreakpointNamesItsSymbolAndAddress)
        {
            DebuggerViewSnapshot  snapshot;



            snapshot.breakpoints.push_back (MakeLine (1, BreakpointKind::Address, 0xFA62));
            snapshot.breakpoints[0].label          = "RESET";
            snapshot.breakpoints[0].info.condition = "A=41";
            snapshot.breakpoints[0].info.hits      = 3;

            Assert::AreEqual (std::string ("RESET $FA62"), Cell (snapshot, 0, Column::Name));
            Assert::AreEqual (std::string ("A=41"),        Cell (snapshot, 0, Column::Condition));
            Assert::AreEqual (std::string ("3"),           Cell (snapshot, 0, Column::HitCount));
            Assert::AreEqual (std::string ("Address"),     Cell (snapshot, 0, Column::Kind));
            Assert::AreEqual (std::string ("$FA62"),       Cell (snapshot, 0, Column::Address));
            Assert::AreEqual (std::string ("RESET"),       Cell (snapshot, 0, Column::Label));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 0, Column::File));
            Assert::AreEqual (std::string ("Break"),       Cell (snapshot, 0, Column::WhenHit));
        }


        TEST_METHOD (ASourceBreakpointNamesItsFileAndLine)
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
            Assert::AreEqual (std::string ("Data write"),  Cell (snapshot, 0, Column::Kind));
            Assert::AreEqual (std::string ("Opcode $EA"),  Cell (snapshot, 1, Column::Name));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 1, Column::Address), L"an opcode stops anywhere");
            Assert::AreEqual (std::string ("X=00"),        Cell (snapshot, 2, Column::Name));
            Assert::AreEqual (std::string (""),            Cell (snapshot, 2, Column::Condition), L"the condition is the name");
            Assert::AreEqual (std::string ("I/O $C030"),   Cell (snapshot, 3, Column::Name));
            Assert::AreEqual (std::string ("BRK"),         Cell (snapshot, 4, Column::Name));
            Assert::AreEqual (std::string ("Interrupt"),   Cell (snapshot, 5, Column::Name));
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

            order = BreakpointColumns::GetOrder (snapshot, Column::Label, false);
            Assert::IsTrue (order == std::vector<size_t> { 2, 0, 1 }, L"by label, ignoring case");
        }


        TEST_METHOD (TheDefaultColumnsAreNameConditionAndHitCount)
        {
            BreakpointColumns::Shown  shown = BreakpointColumns::GetDefaultShown();



            Assert::IsTrue  (shown[(size_t) Column::Name]);
            Assert::IsTrue  (shown[(size_t) Column::Condition]);
            Assert::IsTrue  (shown[(size_t) Column::HitCount]);
            Assert::IsFalse (shown[(size_t) Column::Kind]);
            Assert::AreEqual (std::string (""), BreakpointColumns::FormatShown (shown), L"the default is not written");
            Assert::IsTrue  (BreakpointColumns::ParseShown ("panel=disk2 code2=0300") == shown, L"nothing saved is the default");
        }


        TEST_METHOD (TheChosenColumnsSurviveTheSavedText)
        {
            BreakpointColumns::Shown  shown = BreakpointColumns::GetDefaultShown();
            std::string               text;



            shown[(size_t) Column::HitCount] = false;
            shown[(size_t) Column::File]     = true;
            text                             = "follow=1" + BreakpointColumns::FormatShown (shown) + " memory2=0400";

            Assert::IsTrue (BreakpointColumns::ParseShown (text) == shown);
        }
    };
}
