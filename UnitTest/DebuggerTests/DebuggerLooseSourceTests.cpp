#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DroppedFiles.h"

#include "CppUnitTest.h"

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

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override { return {}; }

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
        std::vector<std::string>             commands;
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
        using DebuggerWindow::OpenSourcePath;
        using DebuggerWindow::LoadSymbolsFor;
        using DebuggerWindow::IsLooseSource;
        using DebuggerWindow::GetSourcePane;
        using DebuggerWindow::IsPaneShown;


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
            Assert::IsFalse (GetSourcePane (slot).GetView()->GetRows().empty(), L"the document has rows");

            for (const std::wstring & cell : GetSourcePane (slot).GetView()->GetRows().front().cells)
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

            window.OpenSourcePath (L"C:\\Work\\main.a65");
            slot = window.GetShownSlot();

            Assert::IsTrue    (slot >= 0);
            Assert::IsTrue    (window.IsLooseSource (slot));
            Assert::AreEqual  (std::wstring (SourcePane::kpszNoSymbolsText), window.GetSourcePane (slot).GetBanner()->GetText());
            Assert::IsNotNull (window.GetSourcePane (slot).GetBanner()->GetAction (0));
            Assert::AreEqual  (std::wstring (SourcePane::kpszLoadSymbols), window.GetSourcePane (slot).GetBanner()->GetAction (0)->GetAccessibleName());
        }


        TEST_METHOD (LoadSymbolsPicksAndLoadsTheSymbolFile)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            LooseSourceHost    host;
            LooseSourceWindow  window (theme, host);



            host.files[L"C:\\Work\\main.a65"] = "start   lda #$41\n";
            host.dialogs.path                 = L"C:\\Elsewhere\\main.dbg";

            window.OpenSourcePath (L"C:\\Work\\main.a65");
            window.LoadSymbolsFor (window.GetShownSlot());

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

            window.OpenSourcePath (L"C:\\Work\\main.a65");

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
            Assert::IsTrue   (window.GetSourcePane (window.GetShownSlot()).GetBanner()->GetText().empty(), L"only a source offers symbols");
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
    };
}
