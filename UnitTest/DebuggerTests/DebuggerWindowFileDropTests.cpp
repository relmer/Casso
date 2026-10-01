#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerWindowFileDropTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FileDropHost
    //
    //  A host that keeps the commands it was given and matches every dropped
    //  file to the first record, as a file whose hash matches would be.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FileDropHost : public IDebuggerWindowHost
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

        void  SetDebuggerMemoryWindow (int window, std::optional<Word> address) override
        {
            if (address.has_value())
            {
                openedMemory.push_back (window);
            }
        }

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { commands.push_back (line); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring & path, const std::string &, int & index) override
        {
            SourceLookup  lookup;

            lookup.match = SourceMatch::Exact;
            lookup.path  = path;
            lookup.text  = "    LDA #1\n";
            index        = 0;
            return lookup;
        }

        std::vector<int>          openedMemory;
        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FileDropWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FileDropWindow : public DebuggerWindow
    {
    public:
        FileDropWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::OnFilesDropped;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::Layout;
        using DebuggerWindow::CloseFloatingPane;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowFileDropTests
    //
    //  A debug or symbol file dropped on the window loads; a source file opens
    //  in the document for the record it matches.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowFileDropTests)
    {
    public:

        TEST_METHOD (DroppingADebugFileLoadsIt)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            FileDropHost    host;
            FileDropWindow  window (theme, host);
            bool            taken  = false;



            window.OnCreate();
            taken = window.OnFilesDropped ({ L"C:\\Work\\game.dbg", L"C:\\Work\\rom.SYM" });

            Assert::IsTrue   (taken);
            Assert::AreEqual ((size_t) 2, host.commands.size());
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\Work\\game.dbg\""), host.commands[0]);
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\Work\\rom.SYM\""),  host.commands[1]);
        }


        TEST_METHOD (DroppingAnyFileWithNoDebugFileLoadedLoadsIt)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            FileDropHost    host;
            FileDropWindow  window (theme, host);



            window.OnCreate();

            Assert::IsTrue   (window.OnFilesDropped ({ L"C:\\Work\\game.lst" }), L"SYM LOAD reads a Merlin listing");
            Assert::AreEqual ((size_t) 1, host.commands.size());
        }


        TEST_METHOD (DroppingASourceFileOpensTheDocumentItMatches)
        {
            CassoTheme                         theme    = CassoTheme::MakeSkeuomorphic();
            FileDropHost                       host;
            FileDropWindow                     window   (theme, host);
            DxuiDpiScaler                      scaler;
            auto                               snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::SourceState  source;
            DebugSourceFile                    file;
            bool                               shown    = false;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            file.id              = 3;
            file.name            = "main.s";
            source.debugFilePath = L"C:\\Work\\game.dbg";
            source.files         = { file };
            snapshot->source     = source;
            window.TakeSnapshot (snapshot);

            Assert::IsTrue (window.OnFilesDropped ({ L"C:\\Elsewhere\\main.s" }));

            for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
            {
                shown = shown || window.IsPaneShown (DebuggerLayout::GetSourcePaneId (slot));
            }

            Assert::IsTrue (shown, L"a document shows the dropped file");
            Assert::IsTrue (host.commands.empty(), L"a source file is not loaded as symbols");
        }
    };
}