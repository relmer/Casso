#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceSyntax.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceLineCodeTests
    //
    //  The source pane shows the instructions each line produced on rows below
    //  it, in darkened syntax colors: the snapshot's instructions per line, the
    //  rows they make, and the source line each row stands for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceLineCodeTests)
    {
    public:
        //  LDA #$41 and STA $0400 from line 2, RTS from line 5, the PC on line 2.
        static void LoadTwoLineFile (ControllerRig & rig)
        {
            static constexpr Byte  s_kCode[] = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x60 };
            DebugFile              file;



            for (size_t i = 0; i < std::size (s_kCode); i++)
            {
                rig.machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), s_kCode[i]);
            }

            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 5 }, { 1, 0, 5, 1 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } },
                           { 1, 0, 5, DebugLineType::Asm, 0, { 1 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        TEST_METHOD (EachLineCarriesTheInstructionsItProduced)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            LoadTwoLineFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsNotNull (snapshot.source->lineCode.get());
            Assert::AreEqual  ((size_t) 2, snapshot.source->lineCode->at ({ 0, 2 }).size());
            Assert::AreEqual  (std::string ("0300  LDA #$41"),  snapshot.source->lineCode->at ({ 0, 2 })[0]);
            Assert::AreEqual  (std::string ("0302  STA $0400"), snapshot.source->lineCode->at ({ 0, 2 })[1]);
            Assert::AreEqual  (std::string ("0305  RTS"),       snapshot.source->lineCode->at ({ 0, 5 })[0]);
        }



        TEST_METHOD (TheInstructionsAreRowsBelowTheirLine)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;
            std::vector<int>                expected = { 1, 2, 0, 0, 3 };



            style.codeSyntax.mnemonic = 0xFF000011;
            style.codeSyntax.address  = 0xFF000022;
            rows = SourcePane::BuildRows ({ L"x", L"  mymacro", L"  .byte 1" }, 0, {}, {}, style, {}, {},
                                          SourceSyntax::Assembler::As65, SourceSyntax::Listing::None,
                                          { { 2, { L"0300  LDA #$41", L"0302  STA $0400" } }, { 3, { L"0305  ORA ($01,X)" } } });

            Assert::AreEqual ((size_t) 5, rows.size(), L"a directive's data has no instruction rows");
            Assert::AreEqual (std::wstring (L"    0300  LDA #$41"), rows[2].cells[2]);
            Assert::IsTrue   (expected == SourcePane::GetRowLines (rows));
            Assert::IsTrue   (std::any_of (rows[2].spans.begin(), rows[2].spans.end(), [] (const DxuiTextView::Span & span)
                              {
                                  return span.cell == 2 && span.start == 4 && span.length == 4 && span.argb == 0xFF000022u;
                              }), L"the address");
            Assert::IsTrue   (std::any_of (rows[2].spans.begin(), rows[2].spans.end(), [] (const DxuiTextView::Span & span)
                              {
                                  return span.cell == 2 && span.start == 10 && span.length == 3 && span.argb == 0xFF000011u;
                              }), L"the mnemonic");
        }
    };
}
