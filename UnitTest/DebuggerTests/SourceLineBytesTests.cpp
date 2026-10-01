#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceLineBytesTests
    //
    //  The source pane shows each line's bytes after it, dimmer than the text:
    //  LDA #$41 at line 2 and STA $0400 at line 3, read from memory while the
    //  machine is stopped.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceLineBytesTests)
    {
    public:
        static void LoadDebugFile (ControllerRig & rig)
        {
            DebugFile  file;



            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } },
                           { 1, 0, 3, DebugLineType::Asm, 0, { 1 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        TEST_METHOD (EachLineCarriesItsBytes)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsNotNull (snapshot.source->lineBytes.get());
            Assert::AreEqual  (std::string ("A9 41"),    snapshot.source->lineBytes->at ({ 0, 2 }));
            Assert::AreEqual  (std::string ("8D 00 04"), snapshot.source->lineBytes->at ({ 0, 3 }));
        }



        TEST_METHOD (TheBytesFollowTheTextDimmer)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.bytesArgb = 0xFF808080;
            rows = SourcePane::BuildRows ({ L"x", L"lda #$41" }, 0, {}, {}, style, { { 2, L"A9 41" } });

            Assert::AreEqual ((size_t) 4,              rows[1].cells.size());
            Assert::AreEqual (std::wstring (L"A9 41"), rows[1].cells[3]);
            Assert::AreEqual (std::wstring(),          rows[0].cells[3], L"a line with no code has an empty column");
            Assert::AreEqual (3,                       rows[1].spans.back().cell);
            Assert::AreEqual (0xFF808080u,             rows[1].spans.back().argb);
        }
    };
}
