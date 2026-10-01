#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSitePaneMenuTests
//
//  The menu of a pane's tab and of a tool window's title bar, as Visual
//  Studio shows it: Dock, Dock in tab group, Auto hide, Move to new window
//  (from a tab only), All to new window, and Close.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSitePaneMenuTests
{
    using MenuItem = DxuiDockSite::MenuItem;

    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        std::wstring     floated;
        std::wstring     closed;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane (L"code",    L"Disassembly", &code);
            site.AddPane (L"regs",    L"Registers",   &regs);
            site.AddPane (L"console", L"Console",     &console);
            site.AddPane (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn       ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetOnFloatRequested ([this] (const std::wstring & pane, POINT) { floated = pane; });
            site.SetOnClosePane      ([this] (const std::wstring & pane) { closed = pane; }, nullptr);
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
        }
    };



    static const MenuItem * Find (const std::vector<MenuItem> & items, const std::wstring & label)
    {
        for (const MenuItem & item : items)
        {
            if (item.label == label)
            {
                return &item;
            }
        }

        return nullptr;
    }



    static void RequireItem (const MenuItem * item)
    {
        if (item == nullptr)
        {
            Assert::Fail (L"the menu holds the item");
        }
    }



    TEST_CLASS (DxuiDockSitePaneMenuTests)
    {
    public:

        TEST_METHOD (ATabMenuHasVisualStudiosItemsInOrder)
        {
            Rig                    rig;
            std::vector<MenuItem>  items    = rig.site.GetPaneMenu (L"console", true);
            const wchar_t        * kOrder[] = { L"Dock", L"Dock in tab group", L"Auto hide", L"Move to new window",
                                                L"All to new window", L"", L"Close" };



            Assert::AreEqual (std::size (kOrder), items.size());

            for (size_t i = 0; i < items.size(); i++)
            {
                Assert::AreEqual (std::wstring (kOrder[i]), items[i].label);
            }

            Assert::IsFalse  (items[0].enabled, L"Dock: already docked");
            Assert::IsTrue   (items[1].enabled);
            Assert::IsTrue   (items[2].enabled);
            Assert::AreEqual (std::wstring (L"Shift+Esc"), items.back().accelerator);
        }


        TEST_METHOD (ATitleBarMenuLeavesOutMoveToNewWindow)
        {
            Rig                    rig;
            std::vector<MenuItem>  items = rig.site.GetPaneMenu (L"console", false);



            Assert::IsNull    (Find (items, L"Move to new window"));
            Assert::IsNotNull (Find (items, L"All to new window"));
        }


        TEST_METHOD (DockInTabGroupTabsTheToolWindowWithTheDocuments)
        {
            Rig                    rig;
            std::vector<MenuItem>  items = rig.site.GetPaneMenu (L"console", true);
            const MenuItem       * item  = Find (items, L"Dock in tab group");
            std::vector<std::wstring>  group;



            RequireItem (item);
            Assert::IsTrue (item->action());

            group = rig.site.GetPaneLayout().GetGroup (L"console");
            Assert::IsTrue (std::find (group.begin(), group.end(), L"code") != group.end());
        }


        TEST_METHOD (ADocumentCannotDockInTabGroup)
        {
            Rig                    rig;
            std::vector<MenuItem>  items = rig.site.GetPaneMenu (L"code", true);
            const MenuItem       * item  = Find (items, L"Dock in tab group");



            RequireItem (item);
            Assert::IsFalse (item->enabled);
        }


        TEST_METHOD (AHiddenPaneDocksFromTheMenu)
        {
            Rig                    rig;
            std::vector<MenuItem>  items;
            const MenuItem       * item = nullptr;



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();

            items = rig.site.GetPaneMenu (L"console", false);
            item  = Find (items, L"Dock");

            RequireItem (item);
            Assert::IsTrue (item->enabled);
            Assert::IsTrue (item->action());
            Assert::IsTrue (rig.site.GetPaneLayout().IsDocked (L"console"));
        }


        TEST_METHOD (MoveToNewWindowFloatsThePane)
        {
            Rig                    rig;
            std::vector<MenuItem>  items = rig.site.GetPaneMenu (L"stack", true);
            const MenuItem       * item  = Find (items, L"Move to new window");



            RequireItem (item);
            Assert::IsTrue   (item->enabled);
            Assert::IsTrue   (item->action());
            Assert::AreEqual (std::wstring (L"stack"), rig.floated);
        }


        TEST_METHOD (AllToNewWindowFloatsALonePane)
        {
            Rig                    rig;
            std::vector<MenuItem>  lone   = rig.site.GetPaneMenu (L"console", false);
            std::vector<MenuItem>  tabbed = rig.site.GetPaneMenu (L"stack", false);
            const MenuItem       * item   = Find (lone, L"All to new window");



            RequireItem (item);
            Assert::IsTrue   (item->enabled);
            Assert::IsTrue   (item->action());
            Assert::AreEqual (std::wstring (L"console"), rig.floated);

            item = Find (tabbed, L"All to new window");
            RequireItem (item);
            Assert::IsFalse (item->enabled, L"a floating window holds one pane");
        }


        TEST_METHOD (CloseClosesThePane)
        {
            Rig                    rig;
            std::vector<MenuItem>  items = rig.site.GetPaneMenu (L"regs", true);
            const MenuItem       * item  = Find (items, L"Close");



            RequireItem (item);
            Assert::IsTrue   (item->enabled);
            Assert::IsTrue   (item->action());
            Assert::AreEqual (std::wstring (L"regs"), rig.closed);
        }
    };
}
