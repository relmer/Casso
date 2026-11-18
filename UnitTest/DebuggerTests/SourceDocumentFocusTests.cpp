#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SourceDocumentFocusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FocusWindow
    //
    //  The debugger window with its controls built and no HWND, whose pane
    //  host reports the focus pane the test sets and the panes it puts in the
    //  documents' tab group.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FocusWindow : public DebuggerWindow
    {
    public:
        FocusWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;

            OnCreate();
        }

        std::wstring  GetFocusPane() const override { return focusPane; }

        bool  IsSameTabGroup (const std::wstring & a, const std::wstring & b) const override
        {
            return sameGroup.contains (a) && sameGroup.contains (b);
        }

        using DebuggerWindow::GetSourceGroup;

        std::wstring            focusPane;
        std::set<std::wstring>  sameGroup;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceDocumentFocusTests
    //
    //  A stop in a source file brings its document to the front of its tab
    //  group, unless the focus is on a pane of that group that isn't a
    //  source document. That pane's tab remains in front.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceDocumentFocusTests)
    {
    public:

        //  A pane in the documents' group that isn't a document remains in
        //  front; a source document with the focus doesn't, and nor
        //  does a pane in another group, or a window with no pane focused.
        TEST_METHOD (OnlyAPaneThatIsNotADocumentKeepsTheFocus)
        {
            CassoTheme              theme    = CassoTheme::MakeSkeuomorphic();
            NullDebuggerWindowHost  host;
            FocusWindow             window   (theme, host);
            std::wstring            code     = DebuggerLayout::GetCodePaneId (0);
            std::wstring            document = DebuggerLayout::GetSourcePaneId (0);
            std::wstring            other    = DebuggerLayout::GetSourcePaneId (1);



            window.sameGroup = { code, document, other };

            window.focusPane = code;
            Assert::IsTrue  (window.GetSourceGroup().IsFocusKept (0), L"the code view in the documents' group remains in front");

            window.focusPane = other;
            Assert::IsFalse (window.GetSourceGroup().IsFocusKept (0), L"another source document doesn't");

            window.focusPane = document;
            Assert::IsFalse (window.GetSourceGroup().IsFocusKept (0), L"nor does the document itself");

            window.focusPane = DebuggerLayout::kRegisters;
            Assert::IsFalse (window.GetSourceGroup().IsFocusKept (0), L"nor a pane in another group");

            window.focusPane.clear();
            Assert::IsFalse (window.GetSourceGroup().IsFocusKept (0), L"nor a window with no pane focused");
        }
    };
}
