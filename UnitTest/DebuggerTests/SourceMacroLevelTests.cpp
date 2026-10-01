#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceMacroLevelTests
    //
    //  Stopped in a macro invoked inside another macro: line 14 invokes store,
    //  whose line 9 invokes clear, whose body line 5 holds the PC. The body's
    //  banner gives line 9, the invocation that produced it, and Show
    //  invocation goes out one level per press, to line 9 and then line 14.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceMacroLevelTests)
    {
    public:
        static DebuggerViewSnapshot MakeNested()
        {
            DebuggerViewSnapshot               snapshot;
            DebuggerViewSnapshot::SourceState  source;



            source.files      = { { 0, "main.s", 0, 0, "", 0 } };
            source.fileId     = 0;
            source.line       = 14;
            source.bodyFileId = 0;
            source.bodyLine   = 5;
            source.depth      = 2;
            source.places     = { { 0, 14 }, { 0, 9 }, { 0, 5 } };

            snapshot.source = source;
            return snapshot;
        }



        static std::string MakeText()
        {
            std::string  text;



            for (int i = 1; i <= 16; i++)
            {
                text += std::format ("line {}\n", i);
            }

            return text;
        }



        static int GetMarkedRow (const DxuiTextView & view)
        {
            for (size_t i = 0; i < view.GetRows().size(); i++)
            {
                if (view.GetRows()[i].cells[0].find (s_kpszTriangleRight) != std::wstring::npos)
                {
                    return (int) i + 1;
                }
            }

            return 0;
        }



        TEST_METHOD (TheBodySaysTheInvocationThatProducedIt)
        {
            DebuggerViewSnapshot  snapshot = MakeNested();
            DxuiTextView          view;
            DxuiActionBanner      banner;
            SourcePane            pane (&view, &banner,
                                        [] (const DebugSourceFile &, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            lookup.match = SourceMatch::Exact;
                                            lookup.text  = MakeText();
                                            return lookup;
                                        },
                                        [] (const std::string &) {},
                                        [] (Word) {});



            pane.SetFile       (0);
            pane.SetMacroLevel (-1);
            pane.Apply         (snapshot);

            Assert::AreEqual (5, GetMarkedRow (view));
            Assert::IsTrue   (banner.GetText().find (L"invoked by line 9.") != std::wstring::npos, banner.GetText().c_str());
            Assert::AreEqual (std::wstring (L"Show invocation"), banner.GetAction (0)->GetAccessibleName());

            pane.SetMacroLevel (1);
            pane.Apply         (snapshot);

            Assert::AreEqual (9, GetMarkedRow (view), L"one level out: the invocation of clear");
            Assert::IsTrue   (banner.GetText().find (L"invoked by line 14.") != std::wstring::npos, banner.GetText().c_str());
            Assert::AreEqual (std::wstring (L"Show invocation"), banner.GetAction (0)->GetAccessibleName());

            pane.SetMacroLevel (0);
            pane.Apply         (snapshot);

            Assert::AreEqual (14, GetMarkedRow (view), L"outside every macro");
            Assert::AreEqual (std::wstring (L"Show body"), banner.GetAction (0)->GetAccessibleName());
        }



        TEST_METHOD (ShowInvocationGoesOutOneLevelPerStep)
        {
            Assert::AreEqual (2, SourcePane::GetNextMacroLevel (0, 2), L"Show body goes to the body line");
            Assert::AreEqual (1, SourcePane::GetNextMacroLevel (2, 2));
            Assert::AreEqual (0, SourcePane::GetNextMacroLevel (1, 2));
            Assert::AreEqual (0, SourcePane::GetNextMacroLevel (0, 0), L"not in a macro");
        }



        TEST_METHOD (TheLevelFollowsTheMacroDepth)
        {
            DebuggerViewSnapshot  snapshot = MakeNested();



            Assert::AreEqual (2, SourcePane::GetMacroLevel (*snapshot.source, -1));
            Assert::AreEqual (2, SourcePane::GetMacroLevel (*snapshot.source, 7), L"a level deeper than the PC is the body");
            Assert::AreEqual (9, SourcePane::GetPlace (*snapshot.source, 1).second);

            snapshot.source->depth = 0;
            Assert::AreEqual (0, SourcePane::GetMacroLevel (*snapshot.source, 2), L"out of the macro");
        }
    };
}
