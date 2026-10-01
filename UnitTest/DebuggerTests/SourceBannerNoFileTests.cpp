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
    //  SourceBannerNoFileTests
    //
    //  With no file open -- the PC outside every source line of the debug file
    //  -- the banner says nothing, rather than " was not found" with no file
    //  name in it. A file gone from disk shows the not-found banner when its
    //  document is opened again.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceBannerNoFileTests)
    {
    public:
        static void LoadDebugFile (ControllerRig & rig)
        {
            DebugFile  file;



            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 2 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        TEST_METHOD (NoFile_ShowsNoNotFoundBanner)
        {
            std::wstring  text = SourcePane::GetBannerText (SourceMatch::NotFound, "", false, 0, false, "", 0);



            Assert::IsTrue (text.empty(), text.c_str());
        }

        TEST_METHOD (AFileThatIsGone_StillShowsTheBanner)
        {
            std::wstring  text = SourcePane::GetBannerText (SourceMatch::NotFound, "a.s", false, 0, false, "", 0);



            Assert::IsTrue (text.starts_with (L"a.s was not found"), text.c_str());
        }

        TEST_METHOD (AFileFoundBesideTheDebugFile_SaysNothingMore)
        {
            Assert::IsTrue (SourcePane::GetFoundElsewhereText ("main.a65", L"C:\\Work\\main.dbg", L"C:\\Work\\main.a65").empty());
            Assert::IsTrue (SourcePane::GetFoundElsewhereText ("main.a65", L"C:\\Work\\main.dbg", L"").empty());
        }

        //  The file renamed away beside the debug file, and an identical copy
        //  in another search folder: its text is shown, and the banner says so.
        TEST_METHOD (ACopyInAnotherFolder_IsSaidToBeACopy)
        {
            ControllerRig         rig;
            DxuiTextView          view;
            DxuiActionBanner      banner;
            SourcePane            pane (&view, &banner,
                                        [] (const DebugSourceFile &, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            lookup.match = SourceMatch::Exact;
                                            lookup.path  = L"C:\\Other\\main.a65";
                                            lookup.text  = "; main\n        lda #$41\n";
                                            return lookup;
                                        },
                                        [] (const DebuggerActionBuilder &) {},
                                        [] (Word) {});



            LoadDebugFile (rig);
            pane.SetFile (0);
            pane.Apply   (rig.view.Build (rig.controller.GetSession()));

            Assert::IsTrue (banner.GetText().find (L"main.a65 is not beside the debug file. Showing the copy in C:\\Other") != std::wstring::npos,
                            banner.GetText().c_str());
        }

        TEST_METHOD (APaneWithNoFile_ShowsNoBanner)
        {
            ControllerRig     rig;
            DxuiTextView      view;
            DxuiActionBanner  banner;
            SourcePane        pane (&view, &banner,
                                    [] (const DebugSourceFile &, const std::wstring &, const std::string &) { return SourceLookup(); },
                                    [] (const DebuggerActionBuilder &) {},
                                    [] (Word) {});



            LoadDebugFile (rig);
            pane.Apply (rig.view.Build (rig.controller.GetSession()));

            Assert::IsTrue (banner.GetText().empty(), banner.GetText().c_str());
        }

        TEST_METHOD (AReopenedDocument_FindsItsFileAgain)
        {
            ControllerRig         rig;
            DxuiTextView          view;
            DxuiActionBanner      banner;
            bool                  onDisk   = true;
            SourcePane            pane (&view, &banner,
                                        [&] (const DebugSourceFile &, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            if (onDisk)
                                            {
                                                lookup.match = SourceMatch::Exact;
                                                lookup.text  = "; main\n        lda #$41\n";
                                            }

                                            return lookup;
                                        },
                                        [] (const DebuggerActionBuilder &) {},
                                        [] (Word) {});
            DebuggerViewSnapshot  snapshot;
            std::vector<int>      lines;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            pane.SetFile (0);
            pane.Apply   (snapshot);
            lines = SourcePane::GetRowLines (view.GetRows());
            Assert::AreEqual ((ptrdiff_t) 2, std::count_if (lines.begin(), lines.end(), [] (int line) { return line > 0; }), L"the file was found");

            pane.SetFile (-1);
            pane.Apply   (snapshot);

            onDisk = false;

            pane.SetFile (0);
            pane.Apply   (snapshot);
            Assert::AreEqual ((size_t) 0, view.GetRows().size(), L"the old text is not shown");
            Assert::IsTrue   (banner.GetText().find (L"main.a65 was not found") != std::wstring::npos, banner.GetText().c_str());
        }
    };
}
