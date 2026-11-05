#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DroppedFiles.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerLooseSourceTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LooseSourceHost
    //
    //  A host whose files are a map of path to content, which keeps the
    //  commands it was given. No dropped file matches a debug file record.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LooseSourceHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { commands.push_back (line); }
        void  RunDebuggerAction        (const DebuggerAction & action)       override { commands.push_back (action.echo); verbs.push_back (action.command.verb); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        //  A debug file's source, by the name in its record.
        SourceLookup  FindDebuggerSource (const DebugSourceFile & record, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;
            auto          found  = sources.find (record.name);



            if (found != sources.end())
            {
                lookup.match = SourceMatch::Exact;
                lookup.path  = L"C:\\Work\\game.s";
                lookup.text  = found->second;
            }

            return lookup;
        }

        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring & path, const std::string &, int & index) override
        {
            SourceLookup  lookup;
            auto          found  = files.find (path);



            index = -1;

            if (found != files.end())
            {
                lookup.path = path;
                lookup.text = found->second;
            }

            return lookup;
        }

        bool  DoesDebuggerFileExist (const std::wstring & path) override { return files.contains (path); }

        std::map<std::wstring, std::string>  files;
        std::map<std::string, std::string>   sources;
        std::vector<std::string>             commands;
        std::vector<DebugVerb>               verbs;
        FakeHostDialogs                      dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LooseSourceWindow
    //
    //  The debugger window with its controls built and no HWND, holding a
    //  snapshot with no debug file loaded.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LooseSourceWindow : public DebuggerWindow
    {
    public:
        LooseSourceWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            DxuiDpiScaler  scaler;



            m_theme = &theme;
            m_host  = &host;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
            TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());
        }

        using DebuggerWindow::OnFilesDropped;
        using DebuggerWindow::GetSourceGroup;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::Layout;

        using DebuggerWindow::ShowNextStatement;
        using DebuggerWindow::GetCodeList;


        //  A stop at a line of game.s, or with no source line for a file of
        //  -1, the first disassembly view open and following the PC.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeStop (int fileId, int line, bool isStepBySource)
        {
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebugSourceFile                        file;



            file.id   = 0;
            file.name = "game.s";

            snapshot->isPaused              = true;
            snapshot->codeOpen[0]           = true;
            snapshot->followView            = 0;
            snapshot->source                = DebuggerViewSnapshot::SourceState();
            snapshot->source->debugFilePath = L"C:\\Work\\game.dbg";
            snapshot->source->programKey    = "game";
            snapshot->source->fileId        = fileId;
            snapshot->source->line          = line;
            snapshot->source->stepBySource  = isStepBySource;
            snapshot->source->files.push_back (file);
            return snapshot;
        }


        //  The first disassembly view made a tab of the first document's
        //  group, so only one of the two is in front.
        void  TabCodeWithSource()
        {
            DxuiPaneLayout  layout = GetDockSite()->GetPaneLayout();
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            Assert::IsTrue (layout.TabWith (DebuggerLayout::GetCodePaneId (0), DebuggerLayout::GetSourcePaneId (0)));
            GetDockSite()->SetPaneLayout (layout);
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }


        //  A pane's tab brought to the front of its group.
        void  Activate (const std::wstring & pane)
        {
            DxuiPaneLayout  layout = GetDockSite()->GetPaneLayout();
            DxuiDpiScaler   scaler;



            scaler.SetDpi (96);
            (void) layout.Activate (pane);
            GetDockSite()->SetPaneLayout (layout);
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }


        //  Whether the first disassembly view's list is in front, rather than
        //  open behind the document's tab.
        bool  IsCodeShown() const { return GetCodeList (0) != nullptr && GetCodeList (0)->IsVisible(); }


        //  Ten lines of source, so every stop's line is in the file.
        static std::string  MakeGameSource()
        {
            std::string  text;



            for (int line = 1; line <= 10; line++)
            {
                text += "        NOP     ; line " + std::to_string (line) + "\n";
            }

            return text;
        }


        //  The slot of the one document shown, or -1.
        int  GetShownSlot() const
        {
            int  shown = -1;



            for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
            {
                shown = IsPaneShown (DebuggerLayout::GetSourcePaneId (slot)) ? slot : shown;
            }

            return shown;
        }


        //  The text of the shown document's first row.
        std::wstring  GetFirstRow() const
        {
            int           slot = GetShownSlot();
            std::wstring  text;



            Assert::IsTrue  (slot >= 0, L"a document is shown");
            Assert::IsFalse (GetSourceGroup().GetDocument (slot).GetModel().GetView()->GetRows().empty(), L"the document has rows");

            for (const std::wstring & cell : GetSourceGroup().GetDocument (slot).GetModel().GetView()->GetRows().front().cells)
            {
                text += cell;
            }

            return text;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerLooseSourceTests
    //
    //  A dropped or opened file goes by its kind: a symbol file loads, a source
    //  opens whether or not a debug file is loaded, and anything else loads as
    //  symbols when it reads as a symbol file, or shows as text or a hex dump.
    //  A source with no symbols offers to load them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerLooseSourceTests)
    {
    public:

        TEST_METHOD (ExtensionsSortIntoSymbolsSourceAndOther)
        {
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\game.dbg")  == DroppedFiles::Kind::Symbols);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\rom.SYM")   == DroppedFiles::Kind::Symbols);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\main.a65")  == DroppedFiles::Kind::Source);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\MAIN.S")    == DroppedFiles::Kind::Source, L"a Merlin source");
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\boot.asm")  == DroppedFiles::Kind::Source);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\GAME.BAS")  == DroppedFiles::Kind::Basic, L"an Applesoft listing");
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\defs.inc")  == DroppedFiles::Kind::Source);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\game.lst")  == DroppedFiles::Kind::Other);
            Assert::IsTrue (DroppedFiles::GetKind (L"C:\\Work\\T.MACROS")  == DroppedFiles::Kind::Other);
        }


        TEST_METHOD (DroppingASourceWithNoDebugFileOpensIt)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\main.a65"] = "start   lda #$41\n";

            Assert::IsTrue   (window.OnFilesDropped ({ L"C:\\Work\\main.a65" }));
            Assert::IsTrue   (host.commands.empty(), L"a source is not loaded as symbols");
            Assert::IsTrue   (window.GetFirstRow().find (L"lda #$41") != std::wstring::npos, window.GetFirstRow().c_str());
        }


        TEST_METHOD (ASourceWithNoSymbolsOffersToLoadThem)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);
            int                slot   = -1;



            host.files[L"C:\\Work\\main.a65"] = "start   lda #$41\n";

            window.GetSourceGroup().OpenSourcePath (L"C:\\Work\\main.a65");
            slot = window.GetShownSlot();

            Assert::IsTrue    (slot >= 0);
            Assert::IsTrue    (window.GetSourceGroup().IsLoose (slot));
            Assert::AreEqual  (std::wstring (SourcePane::kpszNoSymbolsText), window.GetSourceGroup().GetDocument (slot).GetModel().GetBanner()->GetText());
            Assert::IsNotNull (window.GetSourceGroup().GetDocument (slot).GetModel().GetBanner()->GetAction (0));
            Assert::AreEqual  (std::wstring (SourcePane::kpszLoadSymbols), window.GetSourceGroup().GetDocument (slot).GetModel().GetBanner()->GetAction (0)->GetAccessibleName());
        }


        TEST_METHOD (LoadSymbolsPicksAndLoadsTheSymbolFile)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\main.a65"] = "start   lda #$41\n";
            host.dialogs.path                 = L"C:\\Elsewhere\\main.dbg";

            window.GetSourceGroup().OpenSourcePath (L"C:\\Work\\main.a65");
            window.GetSourceGroup().LoadSymbolsFor (window.GetShownSlot());

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\Elsewhere\\main.dbg\""), host.commands[0]);
        }


        TEST_METHOD (OpeningASourceLoadsTheDebugFileBesideItFirst)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\main.a65"] = "start   lda #$41\n";
            host.files[L"C:\\Work\\main.dbg"] = "version\tmajor=2,minor=0\n";

            window.GetSourceGroup().OpenSourcePath (L"C:\\Work\\main.a65");

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\Work\\main.dbg\""), host.commands[0]);
        }


        TEST_METHOD (AnUnknownFileThatReadsAsSymbolsLoads)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\game.info"] = "version\tmajor=2,minor=0\n";

            Assert::IsTrue   (window.OnFilesDropped ({ L"C:\\Work\\game.info" }));
            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\Work\\game.info\""), host.commands[0]);
        }


        TEST_METHOD (AnUnknownTextFileOpensAsText)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\notes.txt"] = "Remember the stack\n";

            Assert::IsTrue   (window.OnFilesDropped ({ L"C:\\Work\\notes.txt" }));
            Assert::IsTrue   (host.commands.empty(), L"text is not loaded as symbols");
            Assert::IsTrue   (window.GetFirstRow().find (L"Remember the stack") != std::wstring::npos, window.GetFirstRow().c_str());
            Assert::IsTrue   (window.GetSourceGroup().GetDocument (window.GetShownSlot()).GetModel().GetBanner()->GetText().empty(), L"only a source offers symbols");
        }


        //  A press in a document's text starts a selection; when the
        //  document goes behind another tab before the button comes up, the
        //  release ends the drag all the same, so the pointer coming back
        //  over the text selects nothing.
        TEST_METHOD (ADragEndsWithItsReleaseAfterItsDocumentIsHidden)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);
            DxuiTextView     * view   = nullptr;
            DxuiPaneLayout     layout;
            DxuiDpiScaler      scaler;
            DxuiMouseEvent     ev;
            int                slot   = -1;



            host.files[L"C:\\Work\\notes.txt"] = "Remember the stack\nand the zero page\n";
            Assert::IsTrue (window.OnFilesDropped ({ L"C:\\Work\\notes.txt" }));

            slot = window.GetShownSlot();
            view = window.GetSourceGroup().GetDocument (slot).GetView();

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { view->GetBounds().left + 80, view->GetBounds().top + 12 };
            Assert::IsTrue (window.GetSourceGroup().RouteMouse (ev));
            Assert::IsTrue (view->IsInteracting(), L"the press starts a selection");

            scaler.SetDpi (96);
            layout = window.GetDockSite()->GetPaneLayout();
            Assert::IsTrue  (layout.TabWith (DebuggerLayout::kConsole, DebuggerLayout::GetSourcePaneId (slot)), L"the console is a tab of the document's group");
            (void) layout.Activate (DebuggerLayout::kConsole);
            window.GetDockSite()->SetPaneLayout (layout);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());
            Assert::IsFalse (view->IsVisible(), L"the document is behind the console");

            ev.kind = DxuiMouseEventKind::Up;
            (void) window.GetSourceGroup().RouteMouse (ev);

            Assert::IsFalse (view->IsInteracting(), L"the release ended the drag");
        }


        //  A loose file is no debug file's: its document stays open with its
        //  text when a debug file loads.
        TEST_METHOD (ALooseFileStaysOpenWhenADebugFileLoads)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost                        host;
            LooseSourceWindow                      window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            host.files[L"C:\\Work\\notes.txt"] = "Remember the stack\n";
            Assert::IsTrue (window.OnFilesDropped ({ L"C:\\Work\\notes.txt" }));

            snapshot->source                = DebuggerViewSnapshot::SourceState();
            snapshot->source->debugFilePath = L"C:\\Work\\game.dbg";
            snapshot->source->programKey    = "game";
            window.TakeSnapshot (snapshot);

            Assert::IsTrue (window.GetFirstRow().find (L"Remember the stack") != std::wstring::npos, L"the loose document keeps its text");
        }


        TEST_METHOD (ABinaryFileOpensAsAHexDump)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\game.bin"] = std::string ("\xA9\x41\x00\x60", 4);

            Assert::IsTrue (window.OnFilesDropped ({ L"C:\\Work\\game.bin" }));
            Assert::IsTrue (host.commands.empty(), L"a binary is not loaded as symbols");
            Assert::IsTrue (window.GetFirstRow().find (L"0000  A9 41 00 60") != std::wstring::npos, window.GetFirstRow().c_str());
        }


        TEST_METHOD (AnUnreadableFileGoesToSymLoad)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            Assert::IsTrue   (window.OnFilesDropped ({ L"C:\\Work\\gone.lst" }));
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"SYM LOAD reports why");
        }


        //  On Show next statement the view of the step mode is brought
        //  forward: the PC's line in its file while steps go by source line.
        TEST_METHOD (ShowNextGoesToTheSourceLineWhileStepsGoBySource)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.sources["game.s"] = LooseSourceWindow::MakeGameSource();
            window.TabCodeWithSource();
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 5, true));
            window.Activate (DebuggerLayout::GetCodePaneId (0));
            Assert::IsTrue (window.IsCodeShown());

            window.ShowNextStatement();

            Assert::IsFalse (window.IsCodeShown(), L"the disassembly is in front");
            Assert::IsTrue  (window.GetSourceGroup().GetDocument (0).GetView()->IsVisible(), L"the PC's document is not in front");
        }


        //  While steps go by instruction, the disassembly is brought forward
        //  on Show next statement, though the PC has a source line.
        TEST_METHOD (ShowNextGoesToTheDisassemblyWhileStepsGoByInstruction)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.sources["game.s"] = LooseSourceWindow::MakeGameSource();
            window.TabCodeWithSource();
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 5, false));
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 6, false));
            Assert::IsFalse (window.IsCodeShown(), L"the stop at a line did not bring the document forward");

            window.ShowNextStatement();

            Assert::IsTrue (window.IsCodeShown(), L"the document is in front");
        }


        //  A stop with no source line after one with a line brings the
        //  disassembly forward; the next stop at a line brings the document
        //  back, though it is the line the PC was at before.
        TEST_METHOD (AStopWithNoSourceLineBringsTheDisassemblyForward)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.sources["game.s"] = LooseSourceWindow::MakeGameSource();
            window.TabCodeWithSource();
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 5, true));
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 6, true));
            Assert::IsFalse (window.IsCodeShown(), L"the stop at a line did not bring the document forward");

            window.TakeSnapshot (LooseSourceWindow::MakeStop (-1, 0, true));
            Assert::IsTrue (window.IsCodeShown(), L"the stop in code with no source left the document in front");

            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 6, true));
            Assert::IsFalse (window.IsCodeShown(), L"the stop back at the line left the disassembly in front");
        }


        //  The disassembly is brought forward on Show next statement when the
        //  PC has no source line, whatever the step mode.
        TEST_METHOD (ShowNextGoesToTheDisassemblyWhenThePcHasNoSourceLine)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.sources["game.s"] = LooseSourceWindow::MakeGameSource();
            window.TabCodeWithSource();
            window.TakeSnapshot (LooseSourceWindow::MakeStop (0, 5, true));
            window.TakeSnapshot (LooseSourceWindow::MakeStop (-1, 0, true));
            window.Activate (DebuggerLayout::GetSourcePaneId (0));
            Assert::IsFalse (window.IsCodeShown());

            window.ShowNextStatement();

            Assert::IsTrue (window.IsCodeShown(), L"the document with no PC line is in front");
        }

    };
}
